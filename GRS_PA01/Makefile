CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -lm

all: MT25091_Part_A_Program_A MT25091_Part_A_Program_B

MT25091_Part_A_Program_A: MT25091_Part_A_Program_A.c MT25091_Part_B_Workers.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

MT25091_Part_A_Program_B: MT25091_Part_A_Program_B.c MT25091_Part_B_Workers.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f MT25091_Part_A_Program_A MT25091_Part_A_Program_B temp_io_test.dat
