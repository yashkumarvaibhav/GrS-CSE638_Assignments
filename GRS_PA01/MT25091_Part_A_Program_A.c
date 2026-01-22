#include "MT25091_Part_B_Workers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <cpu|mem|io> [num_processes]\n", argv[0]);
    return 1;
  }

  char *task_type = argv[1];
  int num_processes = 2; // Default from assignment Part A
  if (argc >= 3) {
    num_processes = atoi(argv[2]);
  }

  for (int i = 0; i < num_processes; ++i) {
    pid_t pid = fork();

    if (pid < 0) {
      perror("Fork failed");
      return 1;
    } else if (pid == 0) {
      // Child process
      if (strcmp(task_type, "cpu") == 0) {
        run_cpu_task();
      } else if (strcmp(task_type, "mem") == 0) {
        run_mem_task();
      } else if (strcmp(task_type, "io") == 0) {
        run_io_task();
      } else {
        fprintf(stderr, "Unknown task type: %s\n", task_type);
        exit(1);
      }
      exit(0); // Child exits after work
    }
  }

  // Parent waits for all children
  for (int i = 0; i < num_processes; ++i) {
    wait(NULL);
  }

  return 0;
}
