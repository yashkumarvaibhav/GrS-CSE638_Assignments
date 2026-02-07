#!/bin/bash
# MT25091_Part_C_Experiment.sh
# Automated Experiment Script for PA02 Network I/O Analysis
# Roll Number: MT25091
#
# This script:
# 1. Compiles all implementations
# 2. Sets up network namespaces (as required by PA02 instructions)
# 3. Runs experiments across message sizes and thread counts
# 4. Collects profiling output with perf stat
# 5. Stores results directly in CSV format (NO subfolders)
#
# Usage: sudo ./MT25091_Part_C_Experiment.sh
# Note: Requires sudo for namespace creation and perf access

# Note: Not using set -e because kill/wait commands may return non-zero

# Configuration
ROLL_NUM="MT25091"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DURATION=3  # Test duration in seconds

# Combined CSV output files (aggregates all experiments)
RESULTS_CSV="${SCRIPT_DIR}/${ROLL_NUM}_Part_B_Results.CSV"
PERFSTATS_CSV="${SCRIPT_DIR}/${ROLL_NUM}_Part_B_PerfStats.CSV"

# Message sizes to test (bytes) - 4 distinct sizes as required
MSG_SIZES=(1024 4096 16384 65536)

# Thread counts to test
THREAD_COUNTS=(1 2 4 8)

# Implementation types
IMPLEMENTATIONS=("A1" "A2" "A3")
IMPL_NAMES=("two_copy" "one_copy" "zero_copy")

# Network namespace configuration
NS_SERVER="ns_server"
NS_CLIENT="ns_client"
VETH_SERVER="veth_srv"
VETH_CLIENT="veth_cli"
SERVER_IP="10.0.0.1"
CLIENT_IP="10.0.0.2"
PORT_BASE=9000

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script requires root privileges for namespace setup."
        log_error "Please run with: sudo $0"
        exit 1
    fi
    log_info "Running as root - OK"
}

# Check perf access
check_perf() {
    sysctl -w kernel.perf_event_paranoid=-1 > /dev/null 2>&1
    if ! perf stat -e cycles ls > /dev/null 2>&1; then
        log_error "perf access denied even after sysctl"
        exit 1
    fi
    log_info "perf access OK"
}

# Compile all implementations
compile_all() {
    log_info "Compiling all implementations..."
    cd "$SCRIPT_DIR"
    make clean > /dev/null 2>&1 || true
    make all
    log_info "Compilation complete."
}

# Setup network namespaces
setup_namespaces() {
    log_info "Setting up network namespaces..."
    
    # Clean up any existing namespaces
    cleanup_namespaces 2>/dev/null || true
    
    # Create namespaces
    ip netns add "$NS_SERVER"
    ip netns add "$NS_CLIENT"
    
    # Create veth pair
    ip link add "$VETH_SERVER" type veth peer name "$VETH_CLIENT"
    
    # Move veth interfaces to namespaces
    ip link set "$VETH_SERVER" netns "$NS_SERVER"
    ip link set "$VETH_CLIENT" netns "$NS_CLIENT"
    
    # Configure server namespace
    ip netns exec "$NS_SERVER" ip addr add "${SERVER_IP}/24" dev "$VETH_SERVER"
    ip netns exec "$NS_SERVER" ip link set "$VETH_SERVER" up
    ip netns exec "$NS_SERVER" ip link set lo up
    
    # Configure client namespace
    ip netns exec "$NS_CLIENT" ip addr add "${CLIENT_IP}/24" dev "$VETH_CLIENT"
    ip netns exec "$NS_CLIENT" ip link set "$VETH_CLIENT" up
    ip netns exec "$NS_CLIENT" ip link set lo up
    
    # Verify connectivity
    if ip netns exec "$NS_CLIENT" ping -c 1 -W 1 "$SERVER_IP" > /dev/null 2>&1; then
        log_info "Namespace connectivity verified (client -> server)"
    else
        log_error "Namespace connectivity failed!"
        exit 1
    fi
    
    log_info "Network namespaces ready: $NS_SERVER ($SERVER_IP) <-> $NS_CLIENT ($CLIENT_IP)"
}

# Cleanup network namespaces
cleanup_namespaces() {
    ip netns del "$NS_SERVER" 2>/dev/null || true
    ip netns del "$NS_CLIENT" 2>/dev/null || true
}

# Initialize CSV files with headers
init_csv_files() {
    log_info "Initializing CSV files..."
    echo "implementation,msg_size,threads,throughput_gbps,avg_latency_us,bytes_sent,bytes_received,messages" > "$RESULTS_CSV"
    echo "implementation,msg_size,threads,cpu_cycles,l1_misses,llc_misses,context_switches" > "$PERFSTATS_CSV"
}

# Run a single experiment using namespaces
run_experiment() {
    local impl_idx=$1
    local impl_name=${IMPL_NAMES[$impl_idx]}
    local impl_part=${IMPLEMENTATIONS[$impl_idx]}
    local msg_size=$2
    local threads=$3
    local port=$4
    
    local server_bin="${SCRIPT_DIR}/${ROLL_NUM}_Part_${impl_part}_Server"
    local client_bin="${SCRIPT_DIR}/${ROLL_NUM}_Part_${impl_part}_Client"
    
    log_info "Running: ${impl_name} | size=${msg_size} | threads=${threads} | port=${port}"
    
    # Start server in server namespace (background)
    ip netns exec "$NS_SERVER" "$server_bin" -p "$port" -s "$msg_size" -t "$threads" > /dev/null 2>&1 &
    local server_pid=$!
    sleep 1
    
    # Run client in client namespace with perf stat
    local tmp_client="/tmp/client_output_$$.txt"
    local tmp_perf="/tmp/perf_output_$$.txt"
    
    ip netns exec "$NS_CLIENT" perf stat -o "$tmp_perf" -e cycles,L1-dcache-load-misses,LLC-load-misses,context-switches \
        "$client_bin" -h "$SERVER_IP" -p "$port" -s "$msg_size" -t "$threads" -d "$DURATION" > "$tmp_client" 2>&1 || true
    
    # Kill server
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    sleep 0.5
    
    # Parse client output
    local throughput=$(grep "Throughput:" "$tmp_client" 2>/dev/null | awk '{print $2}' | head -1 || echo "0")
    local latency=$(grep "Avg Latency:" "$tmp_client" 2>/dev/null | awk '{print $3}' | head -1 || echo "0")
    local bytes_sent=$(grep "Total Bytes Sent:" "$tmp_client" 2>/dev/null | awk '{print $4}' | head -1 || echo "0")
    local bytes_recv=$(grep "Total Bytes Received:" "$tmp_client" 2>/dev/null | awk '{print $4}' | head -1 || echo "0")
    local messages=$(grep "Total Messages:" "$tmp_client" 2>/dev/null | awk '{print $3}' | head -1 || echo "0")
    
    # Handle empty values
    [ -z "$throughput" ] && throughput="0"
    [ -z "$latency" ] && latency="0"
    [ -z "$bytes_sent" ] && bytes_sent="0"
    [ -z "$bytes_recv" ] && bytes_recv="0"
    [ -z "$messages" ] && messages="0"
    
    # Parse perf output
    local cycles=$(grep "cycles" "$tmp_perf" 2>/dev/null | head -1 | awk '{gsub(/,/,""); print $1}' || echo "0")
    local l1=$(grep "L1-dcache-load-misses" "$tmp_perf" 2>/dev/null | awk '{gsub(/,/,""); print $1}' || echo "0")
    local llc=$(grep "LLC-load-misses" "$tmp_perf" 2>/dev/null | awk '{gsub(/,/,""); print $1}' || echo "0")
    local cs=$(grep "context-switches" "$tmp_perf" 2>/dev/null | awk '{gsub(/,/,""); print $1}' || echo "0")
    
    # Append to combined CSV files
    echo "${impl_name},${msg_size},${threads},${throughput},${latency},${bytes_sent},${bytes_recv},${messages}" >> "$RESULTS_CSV"
    echo "${impl_name},${msg_size},${threads},${cycles},${l1},${llc},${cs}" >> "$PERFSTATS_CSV"
    
    # Create/append to parameter-encoded CSV files (one per msg_size × threads combination)
    # Format: MT25091_Part_B_<msgsize>B_<threads>T.CSV
    local param_file="${SCRIPT_DIR}/${ROLL_NUM}_Part_B_${msg_size}B_${threads}T.CSV"
    
    # Add header if file doesn't exist
    if [ ! -f "$param_file" ]; then
        echo "implementation,throughput_gbps,avg_latency_us,bytes_sent,bytes_received,messages,cpu_cycles,l1_misses,llc_misses,context_switches" > "$param_file"
    fi
    
    # Append this implementation's data
    echo "${impl_name},${throughput},${latency},${bytes_sent},${bytes_recv},${messages},${cycles},${l1},${llc},${cs}" >> "$param_file"
    
    # Cleanup temp files
    rm -f "$tmp_client" "$tmp_perf"
    
    echo "  -> Throughput: ${throughput} Gbps, Latency: ${latency} µs"
}

# Main function
main() {
    echo "=============================================="
    echo "  PA02 Network I/O Experiment Script"
    echo "  Roll Number: $ROLL_NUM"
    echo "  Mode: Network Namespace Isolation"
    echo "=============================================="
    echo ""
    
    check_root
    check_perf
    compile_all
    setup_namespaces
    init_csv_files
    
    local port=$PORT_BASE
    local total_experiments=$((${#IMPLEMENTATIONS[@]} * ${#MSG_SIZES[@]} * ${#THREAD_COUNTS[@]}))
    local current=0
    
    log_info "Starting $total_experiments experiments (${DURATION}s each)..."
    log_info "Server namespace: $NS_SERVER ($SERVER_IP)"
    log_info "Client namespace: $NS_CLIENT ($CLIENT_IP)"
    echo ""
    
    for impl_idx in "${!IMPLEMENTATIONS[@]}"; do
        for msg_size in "${MSG_SIZES[@]}"; do
            for threads in "${THREAD_COUNTS[@]}"; do
                ((current++))
                echo "[$current/$total_experiments]"
                run_experiment "$impl_idx" "$msg_size" "$threads" "$port"
                ((port++))
            done
        done
    done
    
    # Cleanup namespaces
    log_info "Cleaning up namespaces..."
    cleanup_namespaces
    
    echo ""
    echo "=============================================="
    log_info "All experiments complete!"
    echo ""
    echo "Combined CSV files:"
    echo "  - $RESULTS_CSV"
    echo "  - $PERFSTATS_CSV"
    echo ""
    echo "Parameter-encoded CSV files (16 files = 4 msg_sizes × 4 thread_counts):"
    ls "${SCRIPT_DIR}/${ROLL_NUM}_Part_B_"*"B_"*"T.CSV" 2>/dev/null
    echo "=============================================="
}

# Run main
main "$@"
