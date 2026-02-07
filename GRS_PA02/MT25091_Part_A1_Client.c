/*
 * MT25091_Part_A1_Client.c
 * Two-Copy TCP Client Implementation (Baseline)
 * Roll Number: MT25091
 *
 * This client uses standard send()/recv() socket primitives which result
 * in two data copies per direction.
 *
 * Features:
 *   - Sends data continuously for a fixed duration
 *   - Multiple client threads to simulate concurrent connections
 *   - Measures throughput and latency at application level
 *   - Runtime parameterization for all settings
 */

#include "MT25091_Common.h"

/* Global state */
static volatile int running = 1;

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
  (void)sig;
  printf("\n[Client] Shutdown signal received. Stopping...\n");
  running = 0;
}

/* Client thread function */
void *client_thread(void *arg) {
  ClientContext *ctx = (ClientContext *)arg;
  Message *msg = NULL;
  char *send_buffer = NULL;
  char *recv_buffer = NULL;
  size_t buffer_size = 0;

  printf("[Client %d] Thread started\n", ctx->client_id);

  /* Allocate message with 8 heap-allocated fields */
  msg = allocate_message(ctx->msg_size);
  if (!msg) {
    fprintf(stderr, "[Client %d] Failed to allocate message\n", ctx->client_id);
    goto cleanup;
  }

  /* Serialize message for sending */
  send_buffer = serialize_message(msg, ctx->msg_size, &buffer_size);
  if (!send_buffer) {
    fprintf(stderr, "[Client %d] Failed to serialize message\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Allocate receive buffer */
  recv_buffer = (char *)malloc(buffer_size);
  if (!recv_buffer) {
    fprintf(stderr, "[Client %d] Failed to allocate recv buffer\n",
            ctx->client_id);
    goto cleanup;
  }

  double start_time = get_time_sec();
  double end_time = start_time + ctx->duration;

  /* Main communication loop */
  while (*(ctx->running) && get_time_sec() < end_time) {
    double send_start = get_time_us();

    /* Send data using send() - TWO COPY OPERATION */
    /* Copy 1: User space -> Kernel socket buffer */
    /* Copy 2: Kernel socket buffer -> Network interface */
    ssize_t bytes_sent = send(ctx->socket_fd, send_buffer, buffer_size, 0);

    if (bytes_sent <= 0) {
      if (bytes_sent < 0 && errno != EPIPE) {
        perror("[Client] send error");
      }
      break;
    }

    ctx->bytes_sent += bytes_sent;
    ctx->messages_sent++;

    /* Receive response using recv() - TWO COPY OPERATION */
    /* Copy 1: Network interface -> Kernel buffer */
    /* Copy 2: Kernel buffer -> User space */
    ssize_t total_recv = 0;
    while (total_recv < (ssize_t)buffer_size && *(ctx->running)) {
      ssize_t bytes_recv = recv(ctx->socket_fd, recv_buffer + total_recv,
                                buffer_size - total_recv, 0);
      if (bytes_recv <= 0) {
        if (bytes_recv < 0) {
          perror("[Client] recv error");
        }
        goto cleanup;
      }
      total_recv += bytes_recv;
    }

    double send_end = get_time_us();

    ctx->bytes_received += total_recv;
    ctx->messages_received++;

    /* Calculate round-trip latency */
    ctx->total_latency_us += (send_end - send_start);
  }

  double actual_duration = get_time_sec() - start_time;

  printf("[Client %d] Completed. Duration: %.2f sec, "
         "Sent: %llu bytes (%llu msgs), Received: %llu bytes (%llu msgs)\n",
         ctx->client_id, actual_duration, ctx->bytes_sent, ctx->messages_sent,
         ctx->bytes_received, ctx->messages_received);

cleanup:
  if (msg)
    free_message(msg);
  if (send_buffer)
    free(send_buffer);
  if (recv_buffer)
    free(recv_buffer);
  close(ctx->socket_fd);

  return NULL;
}

int main(int argc, char *argv[]) {
  char *host;
  int port, threads, duration;
  size_t msg_size;

  /* Parse command line arguments */
  parse_client_args(argc, argv, &host, &port, &msg_size, &threads, &duration);

  /* Setup signal handler */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  printf("==============================================\n");
  printf("  Two-Copy TCP Client (Baseline) - MT25091\n");
  printf("==============================================\n");
  printf("Server: %s:%d\n", host, port);
  printf("Message Size: %zu bytes\n", msg_size);
  printf("Threads: %d\n", threads);
  printf("Duration: %d seconds\n", duration);
  printf("==============================================\n\n");

  /* Allocate thread and context arrays */
  pthread_t *thread_ids = (pthread_t *)malloc(threads * sizeof(pthread_t));
  ClientContext **contexts =
      (ClientContext **)malloc(threads * sizeof(ClientContext *));

  if (!thread_ids || !contexts) {
    HANDLE_ERROR("malloc failed for thread arrays");
  }

  /* Create client threads */
  for (int i = 0; i < threads; i++) {
    /* Create socket and connect */
    int sockfd = create_tcp_socket();

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
      fprintf(stderr, "Invalid address: %s\n", host);
      close(sockfd);
      continue;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
      perror("[Client] connect failed");
      close(sockfd);
      continue;
    }

    /* Create client context */
    contexts[i] = (ClientContext *)malloc(sizeof(ClientContext));
    if (!contexts[i]) {
      close(sockfd);
      continue;
    }

    memset(contexts[i], 0, sizeof(ClientContext));
    contexts[i]->socket_fd = sockfd;
    contexts[i]->client_id = i;
    contexts[i]->msg_size = msg_size;
    contexts[i]->duration = duration;
    contexts[i]->running = &running;

    /* Create thread */
    if (pthread_create(&thread_ids[i], NULL, client_thread, contexts[i]) != 0) {
      perror("[Client] pthread_create failed");
      close(sockfd);
      free(contexts[i]);
      contexts[i] = NULL;
      continue;
    }
  }

  /* Wait for all threads to complete */
  unsigned long long total_bytes_sent = 0;
  unsigned long long total_bytes_recv = 0;
  unsigned long long total_messages = 0;
  double total_latency = 0;
  int active_threads = 0;

  for (int i = 0; i < threads; i++) {
    if (contexts[i]) {
      pthread_join(thread_ids[i], NULL);

      total_bytes_sent += contexts[i]->bytes_sent;
      total_bytes_recv += contexts[i]->bytes_received;
      total_messages += contexts[i]->messages_sent;
      total_latency += contexts[i]->total_latency_us;
      active_threads++;

      free(contexts[i]);
    }
  }

  /* Calculate and print summary statistics */
  printf("\n==============================================\n");
  printf("                    SUMMARY\n");
  printf("==============================================\n");

  if (active_threads > 0 && total_messages > 0) {
    double throughput_gbps = (double)(total_bytes_sent + total_bytes_recv) *
                             8.0 / (duration * 1000000000.0);
    double avg_latency_us = total_latency / total_messages;

    printf("Active Threads: %d\n", active_threads);
    printf("Total Bytes Sent: %llu\n", total_bytes_sent);
    printf("Total Bytes Received: %llu\n", total_bytes_recv);
    printf("Total Messages: %llu\n", total_messages);
    printf("Throughput: %.4f Gbps\n", throughput_gbps);
    printf("Avg Latency: %.2f µs\n", avg_latency_us);
  } else {
    printf("No successful connections.\n");
  }

  printf("==============================================\n");

  free(thread_ids);
  free(contexts);

  return 0;
}
