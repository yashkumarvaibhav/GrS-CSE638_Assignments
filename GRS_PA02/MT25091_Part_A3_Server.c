/*
 * MT25091_Part_A3_Server.c
 * Zero-Copy TCP Server Implementation
 * Roll Number: MT25091
 *
 * This server uses sendmsg() with MSG_ZEROCOPY flag for true zero-copy
 * data transmission. The kernel pins user pages and uses DMA directly.
 *
 * Zero-Copy Kernel Behavior:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     APPLICATION (User Space)                   │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │         Message Buffer (8 heap-allocated fields)        │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │                            │                                   │
 * │                  sendmsg(MSG_ZEROCOPY)                        │
 * │                            ▼                                   │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                     KERNEL SPACE                               │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │              Page Pinning (get_user_pages)              │   │
 * │  │   - User pages are pinned in memory                     │   │
 * │  │   - No copy to kernel buffer                            │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │                            │                                   │
 * │                            ▼                                   │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │                  DMA Controller                         │   │
 * │  │   - Direct Memory Access from user pages               │   │
 * │  │   - Data transferred to NIC ring buffer                │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │                            │                                   │
 * │                   Completion notification                     │
 * │                   (via error queue)                           │
 * │                            ▼                                   │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                     NETWORK INTERFACE                          │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │                    NIC TX Queue                         │   │
 * │  │   - Data sent directly from pinned user pages          │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * Requirements for MSG_ZEROCOPY:
 *   - Socket option SO_ZEROCOPY must be enabled
 *   - Kernel version >= 4.14
 *   - Completion notifications must be handled via error queue
 */

#include "MT25091_Common.h"
#include <linux/errqueue.h>
#include <linux/socket.h>
#include <sys/uio.h>

#ifndef SO_ZEROCOPY
#define SO_ZEROCOPY 60
#endif

#ifndef MSG_ZEROCOPY
#define MSG_ZEROCOPY 0x4000000
#endif

/* Global server configuration */
static ServerConfig server_config;
static pthread_t client_threads[MAX_CLIENTS];
static int num_active_clients = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Signal handler */
void signal_handler(int sig) {
  (void)sig;
  printf("\n[Server] Shutdown signal received. Stopping...\n");
  server_config.running = 0;
}

/* Handle zero-copy completion notifications */
int handle_zerocopy_completion(int sockfd) {
  struct msghdr msg = {0};
  char cbuf[128];
  msg.msg_control = cbuf;
  msg.msg_controllen = sizeof(cbuf);

  int ret = recvmsg(sockfd, &msg, MSG_ERRQUEUE);
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0; /* No notification available */
    }
    return -1;
  }

  /* Process completion notification */
  struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
  if (cm && cm->cmsg_level == SOL_IP && cm->cmsg_type == IP_RECVERR) {
    /* Completion notification received - buffer can be reused */
    return 1;
  }

  return 0;
}

/* Client handler thread function */
void *handle_client(void *arg) {
  ClientContext *ctx = (ClientContext *)arg;
  Message *msg = NULL;
  char *recv_buffer = NULL;
  size_t per_field_size = ctx->msg_size / NUM_STRING_FIELDS;
  if (per_field_size == 0)
    per_field_size = 1;
  size_t total_size = per_field_size * NUM_STRING_FIELDS;

  printf("[Server] Client %d connected (socket: %d)\n", ctx->client_id,
         ctx->socket_fd);

  /* Enable zero-copy mode on socket */
  int one = 1;
  if (setsockopt(ctx->socket_fd, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one)) <
      0) {
    perror("[Server] Warning: SO_ZEROCOPY not supported, falling back");
    /* Continue anyway - will work without zero-copy */
  }

  /* Allocate message with 8 heap-allocated fields */
  msg = allocate_message(ctx->msg_size);
  if (!msg) {
    fprintf(stderr, "[Server] Failed to allocate message for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  recv_buffer = (char *)malloc(total_size);
  if (!recv_buffer) {
    fprintf(stderr, "[Server] Failed to allocate recv buffer for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Setup scatter-gather I/O vectors */
  struct iovec iov[NUM_STRING_FIELDS];
  for (int i = 0; i < NUM_STRING_FIELDS; i++) {
    iov[i].iov_base = msg->field[i];
    iov[i].iov_len = per_field_size;
  }

  struct msghdr send_msg;
  memset(&send_msg, 0, sizeof(send_msg));
  send_msg.msg_iov = iov;
  send_msg.msg_iovlen = NUM_STRING_FIELDS;

  int pending_completions = 0;
  const int max_pending = 8; /* Limit pending zero-copy operations */

  /* Main communication loop */
  while (*(ctx->running)) {
    /* Check for zero-copy completions */
    while (pending_completions > 0) {
      int ret = handle_zerocopy_completion(ctx->socket_fd);
      if (ret > 0) {
        pending_completions--;
      } else {
        break;
      }
    }

    /* Receive data from client */
    ssize_t bytes_recv = recv(ctx->socket_fd, recv_buffer, total_size, 0);

    if (bytes_recv <= 0) {
      if (bytes_recv < 0 && errno != ECONNRESET) {
        perror("[Server] recv error");
      }
      break;
    }

    ctx->bytes_received += bytes_recv;
    ctx->messages_received++;

    /* Wait if too many pending operations */
    while (pending_completions >= max_pending) {
      int ret = handle_zerocopy_completion(ctx->socket_fd);
      if (ret > 0) {
        pending_completions--;
      } else if (ret < 0) {
        break;
      }
    }

    /* Send response using MSG_ZEROCOPY */
    /* ZERO COPY: Kernel pins user pages, DMA directly from user memory */
    ssize_t bytes_sent = sendmsg(ctx->socket_fd, &send_msg, MSG_ZEROCOPY);

    if (bytes_sent <= 0) {
      if (bytes_sent < 0) {
        if (errno == ENOBUFS) {
          /* Too many pending, wait for completions */
          handle_zerocopy_completion(ctx->socket_fd);
          continue;
        }
        if (errno != EPIPE) {
          perror("[Server] sendmsg error");
        }
      }
      break;
    }

    pending_completions++;
    ctx->bytes_sent += bytes_sent;
    ctx->messages_sent++;
  }

  /* Drain remaining completions */
  while (pending_completions > 0) {
    int ret = handle_zerocopy_completion(ctx->socket_fd);
    if (ret > 0) {
      pending_completions--;
    } else {
      break;
    }
  }

  printf("[Server] Client %d disconnected. Sent: %llu bytes, Received: %llu "
         "bytes\n",
         ctx->client_id, ctx->bytes_sent, ctx->bytes_received);

cleanup:
  if (msg)
    free_message(msg);
  if (recv_buffer)
    free(recv_buffer);
  close(ctx->socket_fd);
  free(ctx);

  pthread_mutex_lock(&clients_mutex);
  num_active_clients--;
  pthread_mutex_unlock(&clients_mutex);

  return NULL;
}

int main(int argc, char *argv[]) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);
  int port, max_threads;
  size_t msg_size;

  parse_server_args(argc, argv, &port, &msg_size, &max_threads);

  server_config.port = port;
  server_config.msg_size = msg_size;
  server_config.max_threads = max_threads;
  server_config.running = 1;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  printf("==============================================\n");
  printf("  Zero-Copy TCP Server (MSG_ZEROCOPY) - MT25091\n");
  printf("==============================================\n");
  printf("Port: %d\n", port);
  printf("Message Size: %zu bytes\n", msg_size);
  printf("Max Threads: %d\n", max_threads);
  printf("Optimization: MSG_ZEROCOPY (kernel page pinning)\n");
  printf("==============================================\n\n");

  server_fd = create_tcp_socket();

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    HANDLE_ERROR("bind failed");
  }

  if (listen(server_fd, max_threads) < 0) {
    HANDLE_ERROR("listen failed");
  }

  printf("[Server] Listening on port %d...\n", port);

  int client_id = 0;

  while (server_config.running) {
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (new_socket < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      if (server_config.running) {
        perror("[Server] accept error");
      }
      continue;
    }

    pthread_mutex_lock(&clients_mutex);
    if (num_active_clients >= max_threads) {
      pthread_mutex_unlock(&clients_mutex);
      printf("[Server] Maximum clients reached. Rejecting connection.\n");
      close(new_socket);
      continue;
    }
    num_active_clients++;
    pthread_mutex_unlock(&clients_mutex);

    ClientContext *ctx = (ClientContext *)malloc(sizeof(ClientContext));
    if (!ctx) {
      fprintf(stderr, "[Server] Failed to allocate client context\n");
      close(new_socket);
      pthread_mutex_lock(&clients_mutex);
      num_active_clients--;
      pthread_mutex_unlock(&clients_mutex);
      continue;
    }

    memset(ctx, 0, sizeof(ClientContext));
    ctx->socket_fd = new_socket;
    ctx->client_id = client_id++;
    ctx->msg_size = msg_size;
    ctx->running = &server_config.running;

    if (pthread_create(&client_threads[ctx->client_id % MAX_CLIENTS], NULL,
                       handle_client, ctx) != 0) {
      perror("[Server] pthread_create failed");
      close(new_socket);
      free(ctx);
      pthread_mutex_lock(&clients_mutex);
      num_active_clients--;
      pthread_mutex_unlock(&clients_mutex);
      continue;
    }

    pthread_detach(client_threads[ctx->client_id % MAX_CLIENTS]);
  }

  printf("[Server] Shutting down...\n");
  close(server_fd);

  while (num_active_clients > 0) {
    usleep(100000);
  }

  printf("[Server] Shutdown complete.\n");
  return 0;
}
