# PA02 Network I/O Analysis
## Roll Number: MT25091
## Course: Graduate Systems (CSE638)

---

## Project Overview

This project implements and compares three TCP socket communication mechanisms:

| Part | Implementation | Technique |
|------|----------------|-----------|
| A1 | Two-Copy | `send()`/`recv()` with serialization |
| A2 | One-Copy | `sendmsg()` with iovec scatter-gather |
| A3 | Zero-Copy | `MSG_ZEROCOPY` with completion queue |

---

## Files Structure

```
MT25091_PA02/
├── MT25091_Common.h              # Shared definitions and utilities
├── MT25091_Part_A1_Server.c      # Two-copy server
├── MT25091_Part_A1_Client.c      # Two-copy client
├── MT25091_Part_A2_Server.c      # One-copy server
├── MT25091_Part_A2_Client.c      # One-copy client
├── MT25091_Part_A3_Server.c      # Zero-copy server
├── MT25091_Part_A3_Client.c      # Zero-copy client
├── MT25091_Part_C_Experiment.sh  # Automation script
├── MT25091_Part_D_Plots.py       # Matplotlib plotting
├── MT25091_Part_E_Report.md      # Analysis report
├── MT25091_Part_B_Results.CSV    # Throughput/latency data
├── MT25091_Part_B_PerfStats.CSV  # perf stat metrics
├── Makefile                      # Build configuration
└── README                        # This file
```

---

## Build Instructions

### Compile All

```bash
cd MT25091_PA02
make clean
make all
```

### Compile Individual Parts

```bash
make a1    # Two-copy server and client
make a2    # One-copy server and client
make a3    # Zero-copy server and client
```

---

## Running the Applications

### Command Line Arguments

**Server:**
```bash
./MT25091_Part_A<N>_Server -p <port> -s <msg_size> -t <max_threads>
```

**Client:**
```bash
./MT25091_Part_A<N>_Client -h <host> -p <port> -s <msg_size> -t <threads> -d <duration>
```

### Parameters

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-p` | Port number | 8080 |
| `-s` | Message size in bytes | 4096 |
| `-t` | Thread count (max for server) | 4 |
| `-h` | Server hostname/IP | 127.0.0.1 |
| `-d` | Test duration in seconds | 3 |

---

## Quick Demo Commands

### A1: Two-Copy (Terminal 1 - Server, Terminal 2 - Client)

```bash
# Terminal 1 (Server)
./MT25091_Part_A1_Server -p 8080 -s 4096 -t 4

# Terminal 2 (Client)
./MT25091_Part_A1_Client -h 127.0.0.1 -p 8080 -s 4096 -t 2 -d 3
```

### A2: One-Copy (Terminal 1 - Server, Terminal 2 - Client)

```bash
# Terminal 1 (Server)
./MT25091_Part_A2_Server -p 8081 -s 4096 -t 4

# Terminal 2 (Client)
./MT25091_Part_A2_Client -h 127.0.0.1 -p 8081 -s 4096 -t 2 -d 3
```

### A3: Zero-Copy (Terminal 1 - Server, Terminal 2 - Client)

```bash
# Terminal 1 (Server)
./MT25091_Part_A3_Server -p 8082 -s 4096 -t 4

# Terminal 2 (Client)
./MT25091_Part_A3_Client -h 127.0.0.1 -p 8082 -s 4096 -t 2 -d 3
```

---

## Profiling with perf

### Enable perf access

```bash
sudo sysctl kernel.perf_event_paranoid=-1
```

### Run with perf stat

```bash
# Server with perf
perf stat -e cycles,L1-dcache-load-misses,LLC-load-misses,context-switches \
    ./MT25091_Part_A1_Server -p 8080 -s 4096 -t 4

# Client (separate terminal)
./MT25091_Part_A1_Client -h 127.0.0.1 -p 8080 -s 4096 -t 2 -d 3
```

---

## Generating Plots

```bash
python3 MT25091_Part_D_Plots.py
```

**Output Files:**
- `plot_throughput_vs_msgsize.png`
- `plot_latency_vs_threads.png`
- `plot_cache_misses_vs_msgsize.png`
- `plot_cycles_per_byte.png`

---

## Data Files

### MT25091_Part_B_Results.CSV
Contains throughput and latency measurements:
- `implementation`: two_copy, one_copy, zero_copy
- `msg_size`: 1024, 4096, 16384, 65536 bytes
- `threads`: 1, 2, 4, 8
- `throughput_gbps`: measured throughput
- `avg_latency_us`: average latency in microseconds

### MT25091_Part_B_PerfStats.CSV
Contains perf stat metrics:
- `cpu_cycles`: total CPU cycles
- `l1_misses`: L1 data cache load misses
- `llc_misses`: Last-level cache misses
- `context_switches`: context switch count

---

## Network Namespace Setup (For Isolated Testing)

The experiment script uses network namespaces for isolation:

```bash
# Create namespaces
sudo ip netns add ns_server
sudo ip netns add ns_client

# Create veth pair
sudo ip link add veth0 type veth peer name veth1

# Assign to namespaces
sudo ip link set veth0 netns ns_server
sudo ip link set veth1 netns ns_client

# Configure IPs
sudo ip netns exec ns_server ip addr add 10.0.0.1/24 dev veth0
sudo ip netns exec ns_client ip addr add 10.0.0.2/24 dev veth1

# Bring up interfaces
sudo ip netns exec ns_server ip link set veth0 up
sudo ip netns exec ns_client ip link set veth1 up
sudo ip netns exec ns_server ip link set lo up
sudo ip netns exec ns_client ip link set lo up

# Run server in ns_server
sudo ip netns exec ns_server ./MT25091_Part_A1_Server -p 8080 -s 4096 -t 4

# Run client in ns_client (separate terminal)
sudo ip netns exec ns_client ./MT25091_Part_A1_Client -h 10.0.0.1 -p 8080 -s 4096 -t 2 -d 3

# Cleanup
sudo ip netns del ns_server
sudo ip netns del ns_client
```

---

## Sample Output

```
=== Two-Copy TCP Performance Test ===
Message size: 4096 bytes
Number of threads: 2
Duration: 3 seconds

--- Client Results ---
Total bytes sent: 814006272
Total bytes received: 814006272
Total messages: 198732
Throughput: 4.3414 Gbps
Average latency: 30.13 µs
```

---

## Report

See `MT25091_Part_E_Report.md` for:
- Implementation details
- Analysis of data copy mechanisms
- Kernel behavior diagrams
- Answers to all 6 analysis questions
- AI usage declaration

---

## GitHub Repository

https://github.com/yashkumarvaibhav/GrS-CSE638_Assignments

---

## Author

**Roll Number:** MT25091  
**Course:** CSE638 - Graduate Systems  
**Date:** February 2026
