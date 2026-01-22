#include "MT25091_Part_B_Workers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Roll no ends in 1, so count is 1 * 10^4 = 10000 (Increased as per Rinku Mam's
// permission for visibility)
#define LOOP_COUNT 10000

void run_cpu_task() {
  double result = 0.0;
  for (int i = 0; i < LOOP_COUNT; ++i) {
    // Perform complex mathematical calculations
    // Matrix multiplication simulation
    for (int j = 0; j < 5000; j++) {
      result += sin(j) * cos(j) + tan(j);
      result = sqrt(result > 0 ? result : -result);
    }
  }
  // Prevent optimization
  if (result > 1.0e15)
    printf("Result: %f\n", result);
}

void run_mem_task() {
  // 500 MB size to ensure visible memory usage in top
  size_t size = 500 * 1024 * 1024;
  int *arr = (int *)malloc(size);
  if (!arr) {
    perror("Memory allocation failed");
    return;
  }

  for (int i = 0; i < LOOP_COUNT; ++i) {
    // Random usage to stress memory bandwidth/latency
    // Access strided locations to miss cache
    for (size_t j = 0; j < size / sizeof(int); j += 1024) {
      arr[j] = i + j;
    }
    // Read back
    volatile int temp = 0;
    for (size_t j = 0; j < size / sizeof(int); j += 1024) {
      temp += arr[j];
    }
  }
  free(arr);
}

void run_io_task() {
  const char *filename = "temp_io_test.dat";
  FILE *fp;
  char buffer[1024];
  memset(buffer, 'A', sizeof(buffer));

  // Cleanup previous run if exists
  remove(filename);

  for (int i = 0; i < LOOP_COUNT; ++i) {
    fp = fopen(filename, "ab+"); // Append/Update
    if (!fp) {
      perror("File open failed");
      return;
    }

    // Write chunks
    for (int w = 0; w < 100; w++) {
      if (fwrite(buffer, 1, sizeof(buffer), fp) != sizeof(buffer)) {
        perror("File write failed");
        fclose(fp);
        return;
      }
    }

    // Flush to ensure IO access
    fflush(fp);

    // Read back some data
    fseek(fp, 0, SEEK_SET);
    char read_buf[1024];
    fread(read_buf, 1, sizeof(read_buf), fp);

    fclose(fp);
  }
  remove(filename);
}
