/*
 * MT25091_Part_A1_Server.c
 * Two-Copy TCP Server Implementation (Baseline)
 * Roll Number: MT25091
 *
 * This server uses standard send()/recv() socket primitives which result
 * in two data copies:
 *   1. User space -> Kernel socket buffer (during send)
 *   2. Kernel socket buffer -> Network interface
 *
 * Features:
 *   - Accepts multiple concurrent clients
 *   - One thread per client
 *   - Message structure with 8 dynamically allocated string fields
 *   - Runtime parameterization for message size and thread count
 */

#include "MT25091_Common.h"

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
  char *send_buffer = NULL;
  char *recv_buffer = NULL;
  size_t buffer_size = 0;

  printf("[Server] Client %d connected (socket: %d)\n", ctx->client_id,
         ctx->socket_fd);

  /* Allocate message with 8 heap-allocated fields */
  msg = allocate_message(ctx->msg_size);
  if (!msg) {
    fprintf(stderr, "[Server] Failed to allocate message for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Serialize message for sending */
  send_buffer = serialize_message(msg, ctx->msg_size, &buffer_size);
  if (!send_buffer) {
    fprintf(stderr, "[Server] Failed to serialize message for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Allocate receive buffer */
  recv_buffer = (char *)malloc(buffer_size);
  if (!recv_buffer) {
    fprintf(stderr, "[Server] Failed to allocate recv buffer for client %d\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Main communication loop */
  while (*(ctx->running)) {
    /* Receive data from client using recv() - TWO COPY OPERATION */
    /* Copy 1: Network interface -> Kernel buffer */
    /* Copy 2: Kernel buffer -> User space recv_buffer */
    ssize_t bytes_recv = recv(ctx->socket_fd, recv_buffer, buffer_size, 0);

    if (bytes_recv <= 0) {
      if (bytes_recv < 0 && errno != ECONNRESET) {
        perror("[Server] recv error");
      }
      break; /* Client disconnected or error */
    }

    ctx->bytes_received += bytes_recv;
    ctx->messages_received++;

    /* Send response using send() - TWO COPY OPERATION */
    /* Copy 1: User space send_buffer -> Kernel socket buffer */
    /* Copy 2: Kernel socket buffer -> Network interface */
    ssize_t bytes_sent = send(ctx->socket_fd, send_buffer, buffer_size, 0);

    if (bytes_sent <= 0) {
      if (bytes_sent < 0 && errno != EPIPE) {
        perror("[Server] send error");
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
  /* Free resources */
  if (msg)
    free_message(msg);
  if (send_buffer)
    free(send_buffer);
  if (recv_buffer)
    free(recv_buffer);
  close(ctx->socket_fd);
  free(ctx);

  /* Update active client count */
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
  signal(SIGPIPE, SIG_IGN); /* Ignore broken pipe */

  printf("==============================================\n");
  printf("  Two-Copy TCP Server (Baseline) - MT25091\n");
  printf("==============================================\n");
  printf("Port: %d\n", port);
  printf("Message Size: %zu bytes\n", msg_size);
  printf("Max Threads: %d\n", max_threads);
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

  /* Listen for connections */
  if (listen(server_fd, max_threads) < 0) {
    HANDLE_ERROR("listen failed");
  }

  printf("[Server] Listening on port %d...\n", port);

  int client_id = 0;

  /* Accept client connections */
  while (server_config.running) {
    /* Set socket to non-blocking for graceful shutdown checking */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (new_socket < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue; /* Timeout, check if still running */
      }
      if (server_config.running) {
        perror("[Server] accept error");
      }
      continue;
    }

    /* Check if we can accept more clients */
    pthread_mutex_lock(&clients_mutex);
    if (num_active_clients >= max_threads) {
      pthread_mutex_unlock(&clients_mutex);
      printf("[Server] Maximum clients reached. Rejecting connection.\n");
      close(new_socket);
      continue;
    }
    num_active_clients++;
    pthread_mutex_unlock(&clients_mutex);

    /* Create client context */
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

    /* Create thread for client */
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

    /* Detach thread so it cleans up automatically */
    pthread_detach(client_threads[ctx->client_id % MAX_CLIENTS]);
  }

  printf("[Server] Shutting down...\n");
  close(server_fd);

  /* Wait for remaining clients */
  while (num_active_clients > 0) {
    usleep(100000); /* 100ms */
  }

  printf("[Server] Shutdown complete.\n");
  return 0;
}
