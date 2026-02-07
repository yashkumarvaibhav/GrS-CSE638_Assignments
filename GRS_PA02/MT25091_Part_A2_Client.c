/*
 * MT25091_Part_A2_Client.c
 * One-Copy TCP Client Implementation
 * Roll Number: MT25091
 *
 * This client uses sendmsg() with scatter-gather I/O to reduce one copy.
 * The message fields are sent directly from their heap locations using iovec.
 */

#include "MT25091_Common.h"
#include <sys/uio.h>

/* Global state */
static volatile int running = 1;

/* Signal handler */
void signal_handler(int sig) {
  (void)sig;
  printf("\n[Client] Shutdown signal received. Stopping...\n");
  running = 0;
}

/* Client thread function */
void *client_thread(void *arg) {
  ClientContext *ctx = (ClientContext *)arg;
  Message *msg = NULL;
  char *recv_buffer = NULL;
  size_t per_field_size = ctx->msg_size / NUM_STRING_FIELDS;
  if (per_field_size == 0)
    per_field_size = 1;
  size_t total_size = per_field_size * NUM_STRING_FIELDS;

  printf("[Client %d] Thread started\n", ctx->client_id);

  /* Allocate message with 8 heap-allocated fields */
  msg = allocate_message(ctx->msg_size);
  if (!msg) {
    fprintf(stderr, "[Client %d] Failed to allocate message\n", ctx->client_id);
    goto cleanup;
  }

  /* Allocate receive buffer */
  recv_buffer = (char *)malloc(total_size);
  if (!recv_buffer) {
    fprintf(stderr, "[Client %d] Failed to allocate recv buffer\n",
            ctx->client_id);
    goto cleanup;
  }

  /* Setup scatter-gather I/O vectors for sendmsg */
  struct iovec iov[NUM_STRING_FIELDS];
  for (int i = 0; i < NUM_STRING_FIELDS; i++) {
    iov[i].iov_base = msg->field[i];
    iov[i].iov_len = per_field_size;
  }

  struct msghdr send_hdr;
  memset(&send_hdr, 0, sizeof(send_hdr));
  send_hdr.msg_iov = iov;
  send_hdr.msg_iovlen = NUM_STRING_FIELDS;

  double start_time = get_time_sec();
  double end_time = start_time + ctx->duration;

  /* Main communication loop */
  while (*(ctx->running) && get_time_sec() < end_time) {
    double send_start = get_time_us();

    /* Send data using sendmsg() - ONE COPY OPTIMIZATION */
    /* Data is gathered directly from heap-allocated fields */
    ssize_t bytes_sent = sendmsg(ctx->socket_fd, &send_hdr, 0);

    if (bytes_sent <= 0) {
      if (bytes_sent < 0 && errno != EPIPE) {
        perror("[Client] sendmsg error");
      }
      break;
    }

    ctx->bytes_sent += bytes_sent;
    ctx->messages_sent++;

    /* Receive response */
    ssize_t total_recv = 0;
    while (total_recv < (ssize_t)total_size && *(ctx->running)) {
      ssize_t bytes_recv = recv(ctx->socket_fd, recv_buffer + total_recv,
                                total_size - total_recv, 0);
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
  if (recv_buffer)
    free(recv_buffer);
  close(ctx->socket_fd);

  return NULL;
}

int main(int argc, char *argv[]) {
  char *host;
  int port, threads, duration;
  size_t msg_size;

  parse_client_args(argc, argv, &host, &port, &msg_size, &threads, &duration);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  printf("==============================================\n");
  printf("  One-Copy TCP Client (sendmsg) - MT25091\n");
  printf("==============================================\n");
  printf("Server: %s:%d\n", host, port);
  printf("Message Size: %zu bytes\n", msg_size);
  printf("Threads: %d\n", threads);
  printf("Duration: %d seconds\n", duration);
  printf("Optimization: scatter-gather I/O (iovec)\n");
  printf("==============================================\n\n");

  pthread_t *thread_ids = (pthread_t *)malloc(threads * sizeof(pthread_t));
  ClientContext **contexts =
      (ClientContext **)malloc(threads * sizeof(ClientContext *));

  if (!thread_ids || !contexts) {
    HANDLE_ERROR("malloc failed for thread arrays");
  }

  for (int i = 0; i < threads; i++) {
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

    if (pthread_create(&thread_ids[i], NULL, client_thread, contexts[i]) != 0) {
      perror("[Client] pthread_create failed");
      close(sockfd);
      free(contexts[i]);
      contexts[i] = NULL;
      continue;
    }
  }

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
