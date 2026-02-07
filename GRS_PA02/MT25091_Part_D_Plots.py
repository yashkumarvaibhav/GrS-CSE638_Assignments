#!/usr/bin/env python3
"""
MT25091_Part_D_Plots.py
Plotting Script for PA02 Network I/O Analysis
Roll Number: MT25091

This script generates all required plots using matplotlib with hardcoded values.
Run this script to regenerate the plots during demo.

Data collected using NETWORK NAMESPACE ISOLATION:
- Server in ns_server (10.0.0.1)
- Client in ns_client (10.0.0.2)
- Connected via veth pair

Plots generated:
1. Throughput vs Message Size (Line Graph)
2. Latency vs Thread Count (Line Graph)
3. Cache Misses vs Message Size (Line Graph)
4. CPU Cycles per Byte Transferred (Line Graph)
"""

import matplotlib.pyplot as plt
import numpy as np

# ============================================================================
# HARDCODED EXPERIMENTAL DATA
# From namespace experiments using MT25091_Part_C_Experiment.sh
# ============================================================================

# Message sizes tested (bytes)
MSG_SIZES = [1024, 4096, 16384, 65536]
MSG_SIZE_LABELS = ['1KB', '4KB', '16KB', '64KB']

# Thread counts tested
THREAD_COUNTS = [1, 2, 4, 8]

# System configuration
SYSTEM_CONFIG = """
System: Linux (Ubuntu)
CPU: Intel Core (10th Gen)
Memory: 16GB DDR4
Network: Namespace isolation
  ns_server (10.0.0.1)
  ns_client (10.0.0.2)
Test Duration: 3 seconds
"""

# ============================================================================
# THROUGHPUT DATA (Gbps) - From MT25091_Part_B_Results.CSV
# Data at 1 thread for different message sizes
# ============================================================================

# Two-Copy (baseline) throughput: 1KB, 4KB, 16KB, 64KB (1 thread)
throughput_two_copy = [0.4864, 1.9107, 6.4142, 26.3699]

# One-Copy throughput: 1KB, 4KB, 16KB, 64KB (1 thread)
throughput_one_copy = [0.4764, 2.0457, 7.3903, 20.7566]

# Zero-Copy throughput: 1KB, 4KB, 16KB, 64KB (1 thread)
throughput_zero_copy = [0.2891, 1.1567, 4.2649, 16.5811]

# ============================================================================
# LATENCY DATA (microseconds) - at 16KB message size
# Format: latency[thread_count_index] for threads 1, 2, 4, 8
# ============================================================================

# Two-Copy latency at different thread counts (16KB msg)
latency_two_copy = [40.78, 43.54, 64.66, 97.13]

# One-Copy latency at different thread counts (16KB msg)
latency_one_copy = [35.39, 25.85, 66.69, 88.31]

# Zero-Copy latency at different thread counts (16KB msg)
latency_zero_copy = [59.07, 55.87, 77.76, 129.26]

# ============================================================================
# CACHE MISS DATA - From MT25091_Part_B_PerfStats.CSV
# L1 and LLC cache misses at different message sizes (1 thread)
# ============================================================================

# L1 cache misses at 1KB, 4KB, 16KB, 64KB (1 thread) - from MT25091_Part_B_PerfStats.CSV
l1_two_copy = [61018298, 98413687, 166022002, 468900573]
l1_one_copy = [60106832, 104433781, 193524384, 371898024]
l1_zero_copy = [49607671, 64186836, 109729293, 302944765]

# LLC (Last Level Cache) misses at 1KB, 4KB, 16KB, 64KB (1 thread) - from CSV
llc_two_copy = [165046, 165859, 177906, 4413061]
llc_one_copy = [149514, 134811, 158895, 8687259]
llc_zero_copy = [168817, 194656, 196928, 634073]

# ============================================================================
# CPU CYCLES DATA - From MT25091_Part_B_PerfStats.CSV (1 thread)
# ============================================================================

# Total CPU cycles at 1KB, 4KB, 16KB, 64KB (1 thread) - from MT25091_Part_B_PerfStats.CSV
cycles_two_copy = [3534204433, 4433526806, 4328019630, 7255172477]
cycles_one_copy = [3561178133, 4486849183, 4912728151, 6363464011]
cycles_zero_copy = [3692911095, 4104185916, 4338528037, 7973077586]

# Bytes transferred (from MT25091_Part_B_Results.CSV bytes_sent column, 1 thread)
bytes_two_copy = [91204608, 358256640, 1202667520, 4944363520]
bytes_one_copy = [89321472, 383578112, 1385676800, 3891855360]
bytes_zero_copy = [54213632, 216887296, 799670272, 3108962304]


def plot_throughput_vs_msgsize():
    """Plot 1: Throughput vs Message Size - LINE GRAPH"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    ax.plot(MSG_SIZE_LABELS, throughput_two_copy, 'o-', linewidth=2, markersize=8,
            label='Two-Copy (send/recv)', color='#3498db')
    ax.plot(MSG_SIZE_LABELS, throughput_one_copy, 's-', linewidth=2, markersize=8,
            label='One-Copy (sendmsg)', color='#2ecc71')
    ax.plot(MSG_SIZE_LABELS, throughput_zero_copy, '^-', linewidth=2, markersize=8,
            label='Zero-Copy (MSG_ZEROCOPY)', color='#e74c3c')
    
    ax.set_xlabel('Message Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (Gbps)', fontsize=12, fontweight='bold')
    ax.set_title('Throughput vs Message Size\nPA02 Network I/O Analysis - MT25091 (Namespace Isolation)', 
                 fontsize=14, fontweight='bold')
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3)
    
    # Add data point labels
    for i, (tc, oc, zc) in enumerate(zip(throughput_two_copy, throughput_one_copy, throughput_zero_copy)):
        ax.annotate(f'{tc:.1f}', (MSG_SIZE_LABELS[i], tc), textcoords="offset points", 
                    xytext=(0,8), ha='center', fontsize=8)
    
    # Add system config
    ax.text(0.02, 0.98, SYSTEM_CONFIG.strip(), transform=ax.transAxes, 
            fontsize=7, verticalalignment='top', fontfamily='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    plt.savefig('plot_throughput_vs_msgsize.png', dpi=150, bbox_inches='tight')
    print("Generated: plot_throughput_vs_msgsize.png")
    plt.close()


def plot_latency_vs_threads():
    """Plot 2: Latency vs Thread Count - LINE GRAPH"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    ax.plot(THREAD_COUNTS, latency_two_copy, 'o-', linewidth=2, markersize=8,
            label='Two-Copy (send/recv)', color='#3498db')
    ax.plot(THREAD_COUNTS, latency_one_copy, 's-', linewidth=2, markersize=8,
            label='One-Copy (sendmsg)', color='#2ecc71')
    ax.plot(THREAD_COUNTS, latency_zero_copy, '^-', linewidth=2, markersize=8,
            label='Zero-Copy (MSG_ZEROCOPY)', color='#e74c3c')
    
    ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
    ax.set_ylabel('Average Latency (µs)', fontsize=12, fontweight='bold')
    ax.set_title('Latency vs Thread Count (Message Size: 16KB)\nPA02 Network I/O Analysis - MT25091 (Namespace Isolation)', 
                 fontsize=14, fontweight='bold')
    ax.set_xticks(THREAD_COUNTS)
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3)
    
    # Add data labels
    for i, (tc, oc, zc) in enumerate(zip(latency_two_copy, latency_one_copy, latency_zero_copy)):
        ax.annotate(f'{tc:.1f}', (THREAD_COUNTS[i], tc), textcoords="offset points", 
                    xytext=(0,8), ha='center', fontsize=8)
    
    # Add system config
    ax.text(0.02, 0.98, SYSTEM_CONFIG.strip(), transform=ax.transAxes, 
            fontsize=7, verticalalignment='top', fontfamily='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    plt.savefig('plot_latency_vs_threads.png', dpi=150, bbox_inches='tight')
    print("Generated: plot_latency_vs_threads.png")
    plt.close()


def plot_cache_misses_vs_msgsize():
    """Plot 3: Cache Misses vs Message Size - LINE GRAPH"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # L1 Cache Misses (in millions)
    ax1.plot(MSG_SIZE_LABELS, [m/1e6 for m in l1_two_copy], 'o-', linewidth=2, markersize=8,
             label='Two-Copy', color='#3498db')
    ax1.plot(MSG_SIZE_LABELS, [m/1e6 for m in l1_one_copy], 's-', linewidth=2, markersize=8,
             label='One-Copy', color='#2ecc71')
    ax1.plot(MSG_SIZE_LABELS, [m/1e6 for m in l1_zero_copy], '^-', linewidth=2, markersize=8,
             label='Zero-Copy', color='#e74c3c')
    
    ax1.set_xlabel('Message Size', fontsize=11, fontweight='bold')
    ax1.set_ylabel('L1 Cache Misses (millions)', fontsize=11, fontweight='bold')
    ax1.set_title('L1 Cache Misses vs Message Size', fontsize=12, fontweight='bold')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # LLC (Last Level Cache) Misses (in thousands)
    ax2.plot(MSG_SIZE_LABELS, [m/1e3 for m in llc_two_copy], 'o-', linewidth=2, markersize=8,
             label='Two-Copy', color='#3498db')
    ax2.plot(MSG_SIZE_LABELS, [m/1e3 for m in llc_one_copy], 's-', linewidth=2, markersize=8,
             label='One-Copy', color='#2ecc71')
    ax2.plot(MSG_SIZE_LABELS, [m/1e3 for m in llc_zero_copy], '^-', linewidth=2, markersize=8,
             label='Zero-Copy', color='#e74c3c')
    
    ax2.set_xlabel('Message Size', fontsize=11, fontweight='bold')
    ax2.set_ylabel('LLC Misses (thousands)', fontsize=11, fontweight='bold')
    ax2.set_title('LLC (Last Level Cache) Misses vs Message Size', fontsize=12, fontweight='bold')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    fig.suptitle('Cache Misses Analysis - PA02 Network I/O - MT25091 (Namespace Isolation)', 
                 fontsize=14, fontweight='bold', y=1.02)
    
    plt.tight_layout()
    plt.savefig('plot_cache_misses_vs_msgsize.png', dpi=150, bbox_inches='tight')
    print("Generated: plot_cache_misses_vs_msgsize.png")
    plt.close()


def plot_cycles_per_byte():
    """Plot 4: CPU Cycles per Byte Transferred - LINE GRAPH"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Calculate cycles per byte
    cpb_two_copy = [c/b for c, b in zip(cycles_two_copy, bytes_two_copy)]
    cpb_one_copy = [c/b for c, b in zip(cycles_one_copy, bytes_one_copy)]
    cpb_zero_copy = [c/b for c, b in zip(cycles_zero_copy, bytes_zero_copy)]
    
    ax.plot(MSG_SIZE_LABELS, cpb_two_copy, 'o-', linewidth=2, markersize=8,
            label='Two-Copy (send/recv)', color='#3498db')
    ax.plot(MSG_SIZE_LABELS, cpb_one_copy, 's-', linewidth=2, markersize=8,
            label='One-Copy (sendmsg)', color='#2ecc71')
    ax.plot(MSG_SIZE_LABELS, cpb_zero_copy, '^-', linewidth=2, markersize=8,
            label='Zero-Copy (MSG_ZEROCOPY)', color='#e74c3c')
    
    ax.set_xlabel('Message Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('CPU Cycles per Byte', fontsize=12, fontweight='bold')
    ax.set_title('CPU Cycles per Byte Transferred\nPA02 Network I/O Analysis - MT25091 (Namespace Isolation)', 
                 fontsize=14, fontweight='bold')
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3)
    
    # Add data labels
    for i, (tc, oc, zc) in enumerate(zip(cpb_two_copy, cpb_one_copy, cpb_zero_copy)):
        ax.annotate(f'{tc:.1f}', (MSG_SIZE_LABELS[i], tc), textcoords="offset points", 
                    xytext=(0,8), ha='center', fontsize=8)
    
    # Add system config
    ax.text(0.02, 0.98, SYSTEM_CONFIG.strip(), transform=ax.transAxes, 
            fontsize=7, verticalalignment='top', fontfamily='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    plt.savefig('plot_cycles_per_byte.png', dpi=150, bbox_inches='tight')
    print("Generated: plot_cycles_per_byte.png")
    plt.close()


def generate_all_plots():
    """Generate all required plots"""
    print("=" * 50)
    print("PA02 Network I/O - Generating Plots")
    print("Roll Number: MT25091")
    print("Data Source: Namespace Isolation Experiments")
    print("=" * 50)
    print()
    
    plot_throughput_vs_msgsize()
    plot_latency_vs_threads()
    plot_cache_misses_vs_msgsize()
    plot_cycles_per_byte()
    
    print()
    print("=" * 50)
    print("All plots generated successfully!")
    print("=" * 50)


if __name__ == "__main__":
    generate_all_plots()
