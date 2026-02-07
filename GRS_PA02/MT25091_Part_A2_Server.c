/*
 * MT25091_Part_A2_Server.c
 * One-Copy TCP Server Implementation
 * Roll Number: MT25091
 *
 * This server uses sendmsg() with pre-registered scatter-gather buffers
 * to reduce one copy. The key optimization is:
 *   - Using iovec (scatter-gather I/O) to avoid intermediate serialization copy
 *   - The 8 message fields are sent directly from their heap locations
 *
 * Copy reduction explanation:
 *   - Baseline (send): copy from user buffer -> kernel -> network = 2 copies
 *   - One-copy (sendmsg with iovec): direct DMA from user pages -> 1 copy
 */

#include "MT25091_Part_A_Common.h"
#include <sys/uio.h>

/* Global server configuration */
static ServerConfig server_config;
static pthread_t client_threads[MAX_CLIENTS];
static int num_active_clients = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
  (void)sig;
  printf("\n[Server] Shutdown signal received. Stopping...\n");
  server_config.running = 0;
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

  /* Allocate message with 8 heap-allocated fields */
  msg = allocate_message(ctx->msg_size);
  if (!msg) {
    fprintf(stderr, "[Server] Failed to allocate message for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Allocate receive buffer */
  recv_buffer = (char *)malloc(total_size);
  if (!recv_buffer) {
    fprintf(stderr, "[Server] Failed to allocate recv buffer for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Setup scatter-gather I/O vectors - ONE COPY OPTIMIZATION */
  /* Instead of copying all fields to a contiguous buffer first,
   * we use iovec to directly reference the heap-allocated fields */
  struct iovec iov[NUM_STRING_FIELDS];
  for (int i = 0; i < NUM_STRING_FIELDS; i++) {
    iov[i].iov_base = msg->field[i];
    iov[i].iov_len = per_field_size;
  }

  /* Setup message header for sendmsg() */
  struct msghdr send_msg;
  memset(&send_msg, 0, sizeof(send_msg));
  send_msg.msg_iov = iov;
  send_msg.msg_iovlen = NUM_STRING_FIELDS;

  /* Main communication loop */
  while (*(ctx->running)) {
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

    /* Send response using sendmsg() with scatter-gather I/O */
    /* ONE COPY: Data is gathered directly from heap locations */
    /* The kernel uses the iovec structure to DMA data directly */
    /* WITHOUT first copying to a contiguous kernel buffer */
    ssize_t bytes_sent = sendmsg(ctx->socket_fd, &send_msg, 0);

    if (bytes_sent <= 0) {
      if (bytes_sent < 0 && errno != EPIPE) {
        perror("[Server] sendmsg error");
      }
      break;
    }

    ctx->bytes_sent += bytes_sent;
    ctx->messages_sent++;
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

  /* Parse command line arguments */
  parse_server_args(argc, argv, &port, &msg_size, &max_threads);

  /* Initialize server configuration */
  server_config.port = port;
  server_config.msg_size = msg_size;
  server_config.max_threads = max_threads;
  server_config.running = 1;

  /* Setup signal handlers */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  printf("==============================================\n");
  printf("  One-Copy TCP Server (sendmsg) - MT25091\n");
  printf("==============================================\n");
  printf("Port: %d\n", port);
  printf("Message Size: %zu bytes\n", msg_size);
  printf("Max Threads: %d\n", max_threads);
  printf("Optimization: scatter-gather I/O (iovec)\n");
  printf("==============================================\n\n");

  /* Create server socket */
  server_fd = create_tcp_socket();

  /* Bind to address */
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
