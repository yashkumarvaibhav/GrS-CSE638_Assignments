/*
 * MT25091_Common.h
 * Common definitions for PA02 Network I/O Analysis
 * Roll Number: MT25091
 * 
 * This header contains shared structures, constants, and utility functions
 * used by all client-server implementations (two-copy, one-copy, zero-copy).
 */

#ifndef MT25091_COMMON_H
#define MT25091_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

/* Default configuration values */
#define DEFAULT_PORT        8080
#define DEFAULT_MSG_SIZE    1024
#define DEFAULT_THREADS     4
#define DEFAULT_DURATION    10      /* seconds */
#define MAX_CLIENTS         64
#define NUM_STRING_FIELDS   8       /* Message structure has 8 string fields */

/* Error handling macro */
#define HANDLE_ERROR(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Message structure with 8 dynamically allocated string fields */
typedef struct {
    char *field[NUM_STRING_FIELDS];     /* 8 heap-allocated string fields */
} Message;

/* Configuration structure passed to threads */
typedef struct {
    int socket_fd;
    int client_id;
    size_t msg_size;
    int duration;
    volatile int *running;
    
    /* Statistics */
    unsigned long long bytes_sent;
    unsigned long long bytes_received;
    unsigned long long messages_sent;
    unsigned long long messages_received;
    double total_latency_us;
} ClientContext;

/* Server configuration */
typedef struct {
    int port;
    size_t msg_size;
    int max_threads;
    volatile int running;
} ServerConfig;

/* Timing utilities */
static inline double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

static inline double get_time_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* Message allocation and deallocation */
static inline Message *allocate_message(size_t field_size) {
    Message *msg = (Message *)malloc(sizeof(Message));
    if (!msg) return NULL;
    
    size_t per_field_size = field_size / NUM_STRING_FIELDS;
    if (per_field_size == 0) per_field_size = 1;
    
    for (int i = 0; i < NUM_STRING_FIELDS; i++) {
        msg->field[i] = (char *)malloc(per_field_size);
        if (!msg->field[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                free(msg->field[j]);
            }
            free(msg);
            return NULL;
        }
        /* Fill with pattern data */
        memset(msg->field[i], 'A' + i, per_field_size);
    }
    return msg;
}

static inline void free_message(Message *msg) {
    if (msg) {
        for (int i = 0; i < NUM_STRING_FIELDS; i++) {
            if (msg->field[i]) {
                free(msg->field[i]);
            }
        }
        free(msg);
    }
}

/* Serialize message to contiguous buffer for sending */
static inline char *serialize_message(Message *msg, size_t field_size, size_t *total_size) {
    size_t per_field_size = field_size / NUM_STRING_FIELDS;
    if (per_field_size == 0) per_field_size = 1;
    
    *total_size = per_field_size * NUM_STRING_FIELDS;
    char *buffer = (char *)malloc(*total_size);
    if (!buffer) return NULL;
    
    char *ptr = buffer;
    for (int i = 0; i < NUM_STRING_FIELDS; i++) {
        memcpy(ptr, msg->field[i], per_field_size);
        ptr += per_field_size;
    }
    return buffer;
}

/* Create a TCP socket with options */
static inline int create_tcp_socket(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        HANDLE_ERROR("socket creation failed");
    }
    
    /* Enable address reuse */
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        HANDLE_ERROR("setsockopt SO_REUSEADDR failed");
    }
    
    /* Disable Nagle's algorithm for low latency */
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        HANDLE_ERROR("setsockopt TCP_NODELAY failed");
    }
    
    return sockfd;
}

/* Print usage information */
static inline void print_usage(const char *program, int is_server) {
    if (is_server) {
        printf("Usage: %s [-p port] [-s msg_size] [-t max_threads]\n", program);
        printf("Options:\n");
        printf("  -p port        Server port (default: %d)\n", DEFAULT_PORT);
        printf("  -s msg_size    Message size in bytes (default: %d)\n", DEFAULT_MSG_SIZE);
        printf("  -t max_threads Maximum concurrent client threads (default: %d)\n", DEFAULT_THREADS);
    } else {
        printf("Usage: %s [-h host] [-p port] [-s msg_size] [-t threads] [-d duration]\n", program);
        printf("Options:\n");
        printf("  -h host        Server hostname/IP (default: 127.0.0.1)\n");
        printf("  -p port        Server port (default: %d)\n", DEFAULT_PORT);
        printf("  -s msg_size    Message size in bytes (default: %d)\n", DEFAULT_MSG_SIZE);
        printf("  -t threads     Number of client threads (default: %d)\n", DEFAULT_THREADS);
        printf("  -d duration    Test duration in seconds (default: %d)\n", DEFAULT_DURATION);
    }
}

/* Parse command line arguments for server */
static inline void parse_server_args(int argc, char *argv[], int *port, 
                                      size_t *msg_size, int *max_threads) {
    *port = DEFAULT_PORT;
    *msg_size = DEFAULT_MSG_SIZE;
    *max_threads = DEFAULT_THREADS;
    
    int opt;
    while ((opt = getopt(argc, argv, "p:s:t:")) != -1) {
        switch (opt) {
            case 'p':
                *port = atoi(optarg);
                break;
            case 's':
                *msg_size = (size_t)atol(optarg);
                break;
            case 't':
                *max_threads = atoi(optarg);
                break;
            default:
                print_usage(argv[0], 1);
                exit(EXIT_FAILURE);
        }
    }
}

/* Parse command line arguments for client */
static inline void parse_client_args(int argc, char *argv[], char **host, int *port,
                                      size_t *msg_size, int *threads, int *duration) {
    *host = "127.0.0.1";
    *port = DEFAULT_PORT;
    *msg_size = DEFAULT_MSG_SIZE;
    *threads = DEFAULT_THREADS;
    *duration = DEFAULT_DURATION;
    
    int opt;
    while ((opt = getopt(argc, argv, "h:p:s:t:d:")) != -1) {
        switch (opt) {
            case 'h':
                *host = optarg;
                break;
            case 'p':
                *port = atoi(optarg);
                break;
            case 's':
                *msg_size = (size_t)atol(optarg);
                break;
            case 't':
                *threads = atoi(optarg);
                break;
            case 'd':
                *duration = atoi(optarg);
                break;
            default:
                print_usage(argv[0], 0);
                exit(EXIT_FAILURE);
        }
    }
}

#endif /* MT25091_COMMON_H */
