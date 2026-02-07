# Makefile for PA02 Network I/O Analysis
# Roll Number: MT25091
# 
# This Makefile compiles all client-server implementations for the
# PA02 Network I/O assignment.

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

# Source files
COMMON_HEADER = MT25091_Common.h

# Two-Copy Implementation (A1)
A1_SERVER = MT25091_Part_A1_Server
A1_CLIENT = MT25091_Part_A1_Client

# One-Copy Implementation (A2)
A2_SERVER = MT25091_Part_A2_Server
A2_CLIENT = MT25091_Part_A2_Client

# Zero-Copy Implementation (A3)
A3_SERVER = MT25091_Part_A3_Server
A3_CLIENT = MT25091_Part_A3_Client

# All targets
TARGETS = $(A1_SERVER) $(A1_CLIENT) $(A2_SERVER) $(A2_CLIENT) $(A3_SERVER) $(A3_CLIENT)

.PHONY: all clean help

# Default target - build all
all: $(TARGETS)
	@echo "Build complete. All binaries created successfully."
	@echo "Available binaries: $(TARGETS)"

# Two-Copy (A1)
$(A1_SERVER): $(A1_SERVER).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

$(A1_CLIENT): $(A1_CLIENT).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# One-Copy (A2)
$(A2_SERVER): $(A2_SERVER).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

$(A2_CLIENT): $(A2_CLIENT).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Zero-Copy (A3)
$(A3_SERVER): $(A3_SERVER).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

$(A3_CLIENT): $(A3_CLIENT).c $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Clean build artifacts
clean:
	rm -f $(TARGETS)
	@echo "Cleaned all binaries."

# Help
help:
	@echo "PA02 Network I/O Makefile - MT25091"
	@echo "===================================="
	@echo "Targets:"
	@echo "  all     - Build all client and server binaries"
	@echo "  clean   - Remove all binaries"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Binaries:"
	@echo "  $(A1_SERVER) / $(A1_CLIENT) - Two-Copy (baseline)"
	@echo "  $(A2_SERVER) / $(A2_CLIENT) - One-Copy (sendmsg with iovec)"
	@echo "  $(A3_SERVER) / $(A3_CLIENT) - Zero-Copy (MSG_ZEROCOPY)"
