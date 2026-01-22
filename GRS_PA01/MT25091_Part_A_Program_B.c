#include "MT25091_Part_B_Workers.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *thread_func(void *arg) {
  char *task_type = (char *)arg;
  if (strcmp(task_type, "cpu") == 0) {
    run_cpu_task();
  } else if (strcmp(task_type, "mem") == 0) {
    run_mem_task();
  } else if (strcmp(task_type, "io") == 0) {
    run_io_task();
  } else {
    fprintf(stderr, "Unknown task type: %s\n", task_type);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <cpu|mem|io> [num_threads]\n", argv[0]);
    return 1;
  }

  char *task_type = argv[1];
  int num_threads = 2; // Default from assignment Part A
  if (argc >= 3) {
    num_threads = atoi(argv[2]);
  }

  pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
  if (!threads) {
    perror("Malloc failed");
    return 1;
  }

  for (int i = 0; i < num_threads; ++i) {
    if (pthread_create(&threads[i], NULL, thread_func, (void *)task_type) !=
        0) {
      perror("Thread creation failed");
      return 1;
    }
  }

  for (int i = 0; i < num_threads; ++i) {
    pthread_join(threads[i], NULL);
  }

  free(threads);
  return 0;
}
