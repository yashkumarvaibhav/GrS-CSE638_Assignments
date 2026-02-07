Roll No: MT25091  
Name: Yash Kumar Vaibhav  

---

# GRS PA01: Processes and Threads

## Overview
This submission contains the implementation for Assignment 1. It compares the performance of Process-based (fork) and Thread-based (pthreads) applications performing CPU, Memory, and I/O intensive tasks.

## Directory Structure
- `MT25091_Part_A_Program_A.c`: Program using `fork()` to create child processes.
- `MT25091_Part_A_Program_B.c`: Program using `pthread_create()` to create threads.
- `MT25091_Part_B_Workers.c`: Implementation of CPU, Mem, and IO worker functions.
- `MT25091_Part_B_Workers.h`: Header file for workers.
- `MT25091_Part_C_shell.sh`: Bash script to automate execution, measurement, and plotting for Part C and D.
- `MT25091_Part_D_Plot.py`: Python script (called by shell script) to generate PNG plots from CSV data.
- `MT25091_Part_C_CSV.csv`: Generated measurement results for Part C.
- `MT25091_Part_D_CSV.csv`: Generated measurement results for Part D.
- `MT25091_Part_C_Plot.png`: Comparison plot for Part C (Processes vs Threads).
- `MT25091_Part_D_Plot_1.png`: Scaling analysis plot (3x4 grid: CPU, Memory, I/O metrics).
- `MT25091_Part_D_Plot_2.png`: Per-worker memory efficiency plot.
- `MT25091_Report.pdf`: Final report with analysis and observations.
- `Makefile`: Build script for compiling C programs.

---

## Prerequisites
Before running, ensure you have:
- **GCC** compiler with pthread support
- **Python 3** with `matplotlib` library (`pip install matplotlib`)
- **Linux utilities**: `top`, `iostat`, `taskset`, `bc`

---

## Compilation
To compile the programs, run:
```bash
make
```
This generates two executables:
- `MT25091_Part_A_Program_A` (Process-based)
- `MT25091_Part_A_Program_B` (Thread-based)

To clean compiled files:
```bash
make clean
```

---

## Usage

### Manual Execution

**Run Program A (Processes):**
```bash
./MT25091_Part_A_Program_A <task_type> [num_processes]
```
- `<task_type>`: `cpu`, `mem`, or `io` (required)
- `[num_processes]`: Number of child processes to create (default: 2)

**Examples:**
```bash
./MT25091_Part_A_Program_A cpu       # 2 processes running CPU task
./MT25091_Part_A_Program_A mem 4     # 4 processes running Memory task
./MT25091_Part_A_Program_A io 3      # 3 processes running I/O task
```

**Run Program B (Threads):**
```bash
./MT25091_Part_A_Program_B <task_type> [num_threads]
```
- `<task_type>`: `cpu`, `mem`, or `io` (required)
- `[num_threads]`: Number of worker threads to create (default: 2)

**Examples:**
```bash
./MT25091_Part_A_Program_B cpu       # 2 threads running CPU task
./MT25091_Part_A_Program_B mem 5     # 5 threads running Memory task
./MT25091_Part_A_Program_B io 8      # 8 threads running I/O task
```

### Automated Measurement & Plotting (Part C & D)

To run all measurements and generate CSV + PNG files automatically:
```bash
./MT25091_Part_C_shell.sh [cpu_list]
```
- `[cpu_list]`: Optional CPU core(s) to pin execution (default: 0). Example: `0,1` for cores 0 and 1.

**What it does:**
1. Runs all 6 combinations (Program A/B × cpu/mem/io) for Part C
2. Runs scaling tests (Processes: 2-5, Threads: 2-8) for Part D
3. Collects CPU%, Memory%, I/O%, and Duration using `top` and `iostat`
4. Outputs: `MT25091_Part_C_CSV.csv` and `MT25091_Part_D_CSV.csv`
5. Calls `MT25091_Part_D_Plot.py` to generate PNG plots automatically

**Example:**
```bash
./MT25091_Part_C_shell.sh 0      # Pin to CPU 0
./MT25091_Part_C_shell.sh 0,1    # Pin to CPUs 0 and 1
```

---

## Implementation Details

### Part A: Process and Thread Programs

**Program A (Processes):**
- Uses `fork()` in a loop to create N child processes
- Each child runs the specified worker function independently
- Parent waits for all children using `wait()`
- Each process has its own isolated memory space

**Program B (Threads):**
- Uses `pthread_create()` to spawn N worker threads
- All threads share the same process address space
- Main thread waits for all workers using `pthread_join()`
- Threads share heap memory but have individual stacks

### Part B: Worker Functions

- **Loop Count**: 10,000 iterations (increased from 1,000 with Professor's approval for measurable execution time)
- **CPU Task (`cpu`)**: Nested loops with trigonometric calculations (sin/cos/tan/sqrt) - pure computation
- **Memory Task (`mem`)**: Allocates 500MB array, performs strided read/write (stride=1024) to stress memory bandwidth
- **I/O Task (`io`)**: Writes 100KB chunks to temp file, calls `fflush()`, and reads back - disk I/O intensive

### Part C: Baseline Measurement

- Measures CPU%, Memory%, I/O%, and Duration for all 6 variants (A+cpu, A+mem, A+io, B+cpu, B+mem, B+io)
- Uses `top -b -n 2 -d 0.2` in batch mode to capture CPU delta
- Uses `iostat -dx 1 1` to capture disk utilization
- Polling interval: 0.1 seconds
- Results saved to `MT25091_Part_C_CSV.csv`
- Plot generated: `MT25091_Part_C_Plot.png` (2x2 comparison grid)

### Part D: Scaling Analysis

- Increments worker count: Processes (N=2,3,4,5), Threads (N=2,3,4,5,6,7,8)
- Measures performance at each scale for all 3 task types
- Results saved to `MT25091_Part_D_CSV.csv`
- Plots generated:
  - `MT25091_Part_D_Plot_1.png`: 3x4 grid (Rows: Task Type, Cols: Metric)
  - `MT25091_Part_D_Plot_2.png`: Per-worker memory efficiency comparison

---

## AI Declaration

I utilized an AI coding assistant (Google Deepmind's Agent) to:
- Assist in writing few parts of the Bash script for automation.
- Assist in writing Matplotlib code for visualization.
- Write the LaTeX code for generating the report (while the content, data analysis, and logic are my own).

The core C implementation (`fork`, `pthread`, workers) and experimental design are my own work.

---
---

# GRS PA02: Network I/O Analysis

## Overview
This submission contains the implementation for Assignment 2. It compares the performance of three TCP socket communication mechanisms:
- **Two-Copy**: Standard `send()`/`recv()` with serialization
- **One-Copy**: `sendmsg()` with scatter-gather I/O (iovec)
- **Zero-Copy**: `sendmsg()` with `MSG_ZEROCOPY` flag

## Directory Structure
- `MT25091_Part_A_Common.h`: Shared definitions and utilities
- `MT25091_Part_A1_Server.c`: Two-copy server implementation
- `MT25091_Part_A1_Client.c`: Two-copy client implementation
- `MT25091_Part_A2_Server.c`: One-copy server implementation
- `MT25091_Part_A2_Client.c`: One-copy client implementation
- `MT25091_Part_A3_Server.c`: Zero-copy server implementation
- `MT25091_Part_A3_Client.c`: Zero-copy client implementation
- `MT25091_Part_C_Experiment.sh`: Bash script for automated experiments
- `MT25091_Part_D_Plots.py`: Matplotlib script for plot generation
- `MT25091_Part_B_Results.CSV`: Combined throughput/latency data
- `MT25091_Part_B_PerfStats.CSV`: Combined perf stat metrics
- `MT25091_Part_B_<size>B_<threads>T.CSV`: Parameter-encoded CSV files (16 total)
- `MT25091_Part_E_Report.pdf`: Final report with analysis
- `Makefile`: Build configuration
- `README`: This documentation file

---

## Prerequisites
- **GCC** compiler with pthread support
- **Python 3** with `matplotlib` library
- **Linux perf** tool (`sudo apt install linux-tools-common`)
- **Network namespace** support (for isolated testing)

---

## Compilation
```bash
cd GRS_PA02
make clean
make all
```

This generates six executables:
- `MT25091_Part_A1_Server`, `MT25091_Part_A1_Client` (Two-copy)
- `MT25091_Part_A2_Server`, `MT25091_Part_A2_Client` (One-copy)
- `MT25091_Part_A3_Server`, `MT25091_Part_A3_Client` (Zero-copy)

---

## Usage

### Command Line Arguments

**Server:**
```bash
./MT25091_Part_A<N>_Server -p <port> -s <msg_size> -t <max_threads>
```

**Client:**
```bash
./MT25091_Part_A<N>_Client -h <host> -p <port> -s <msg_size> -t <threads> -d <duration>
```

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-p` | Port number | 8080 |
| `-s` | Message size in bytes | 4096 |
| `-t` | Thread count | 4 |
| `-h` | Server hostname/IP | 10.0.0.1 |
| `-d` | Test duration (seconds) | 3 |

### Quick Demo
```bash
# Terminal 1 (Server)
./MT25091_Part_A1_Server -p 8080 -s 4096 -t 4

# Terminal 2 (Client)
./MT25091_Part_A1_Client -h 127.0.0.1 -p 8080 -s 4096 -t 2 -d 3
```

### Automated Experiments
```bash
# Enable perf access
sudo sysctl kernel.perf_event_paranoid=-1

# Run all 48 experiments with namespace isolation
sudo ./MT25091_Part_C_Experiment.sh
```

---

## Generating Plots
```bash
python3 MT25091_Part_D_Plots.py
```
Plots are generated and displayed. Values are hardcoded from CSV data.

---

## AI Declaration

I utilized AI coding assistants (Google Deepmind, GitHub Copilot) to:
- Assist in socket implementation planning
- Debug experiment script errors
- Convert report to LaTeX format
- Generate ASCII diagrams for kernel behavior

The core C implementation, experimental design, and analysis are my own work.

---

## Author

**Roll Number:** MT25091  
**Course:** CSE638 - Graduate Systems  
**Date:** February 2026
