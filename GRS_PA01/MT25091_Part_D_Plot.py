import csv
import matplotlib.pyplot as plt
import sys
import os

def plot_part_c(csv_file):
    if not os.path.exists(csv_file):
        print(f"File {csv_file} not found.")
        return

    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f, skipinitialspace=True)
        for row in reader:
            data.append(row)

    # Prepare data for plotting
    tasks = ['cpu', 'mem', 'io']
    progs = ['Program_A', 'Program_B']
    metrics = ['Duration', 'CPU_Usage', 'Mem_Usage', 'IO_Usage']
    titles = ['Execution Time (s)', 'CPU Usage (%)', 'Memory Usage (%)', 'IO Usage (%)']
    
    # Store by (Prog, Task) -> {Metric: Value}
    results = {}
    for row in data:
        variant = row['Program_Variant']
        if '+' in variant:
            prog, task = variant.split('+')
            results[(prog, task)] = {m: float(row[m]) for m in metrics}

    # Plot Comparison 2x2 Grid
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    x = range(len(tasks))
    width = 0.35
    
    for idx, metric in enumerate(metrics):
        row = idx // 2
        col = idx % 2
        ax = axes[row][col]
        
        prog_a_vals = [results.get(('Program_A', t), {}).get(metric, 0) for t in tasks]
        prog_b_vals = [results.get(('Program_B', t), {}).get(metric, 0) for t in tasks]
        
        rects1 = ax.bar([i - width/2 for i in x], prog_a_vals, width, label='Processes (A)')
        rects2 = ax.bar([i + width/2 for i in x], prog_b_vals, width, label='Threads (B)')
        
        ax.set_ylabel(titles[idx])
        ax.set_title(f'Part C: {titles[idx]} Comparison')
        ax.set_xticks(x)
        ax.set_xticklabels([t.upper() for t in tasks])
        
        # Add labels on bars
        ax.bar_label(rects1, padding=3, fmt='%.1f')
        ax.bar_label(rects2, padding=3, fmt='%.1f')
        
        ax.grid(True, axis='y', linestyle='--', alpha=0.7)
        
        if idx == 0: # Legend only on first plot
            ax.legend()

    plt.suptitle('Part C: Process vs Thread Performance Analysis', fontsize=16)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig('MT25091_Part_C_Plot.png')
    print("Generated MT25091_Part_C_Plot.png with 4 subplots")

def plot_part_d(csv_file):
    if not os.path.exists(csv_file):
        print(f"File {csv_file} not found.")
        return

    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f, skipinitialspace=True)
        for row in reader:
            data.append(row)

    # Data structure:
    # Program_A (Processes) -> {Count: Time}
    # Program_B (Threads) -> {Count: Time}
    
    # Data structure: Task -> {Prog -> {Count -> {Metric: Value}}}
    # We want to plot 4 metrics: Duration, CPU, Mem, IO
    metrics_to_plot = ['Duration', 'CPU_Usage', 'Mem_Usage', 'IO_Usage']
    
    parsed_data = {} 
    # Structure: parsed_data[task][metric][prog][count] = value
    
    for task in ['cpu', 'mem', 'io']:
        parsed_data[task] = {}
        for m in metrics_to_plot:
            parsed_data[task][m] = {'Program_A': {}, 'Program_B': {}}

    for row in data:
        # Format: Configuration (Program_A_Scaling_TASK_COUNT), Duration, CPU_Usage...
        config = row['Configuration']
        
        parts = config.split('_')
        if len(parts) >= 5:
            prog = f"{parts[0]}_{parts[1]}"
            task = parts[-2]
            count = int(parts[-1])
            
            if task in parsed_data:
                for m in metrics_to_plot:
                     parsed_data[task][m][prog][count] = float(row[m])

    # Create plots
    # We will create ONE big figure with rows=Tasks, cols=Metrics? 
    # That's 3x4 = 12 plots. Too many.
    # User Request: "Plot the measured metrics in a way that makes sense"
    # Better approach: 1 Figure per TASK. Each Figure has 4 subplots (Time, CPU, Mem, IO).
    # Since we need one file, let's make 3 rows (Tasks) x 4 columns (Metrics). Size it huge.
    
    fig, axes = plt.subplots(3, 4, figsize=(24, 15))
    tasks = ['cpu', 'mem', 'io']
    titles = ['Time (s)', 'CPU (%)', 'Mem (%)', 'IO (%)']
    ylabels = ['Seconds', 'Percent', 'Percent', 'Percent']
    
    for i, task in enumerate(tasks):
        for j, metric in enumerate(metrics_to_plot):
            ax = axes[i][j]
            
            # Get data
            prog_a = parsed_data[task][metric]['Program_A']
            prog_b = parsed_data[task][metric]['Program_B']
            
            # Sort
            xa = sorted(prog_a.keys())
            ya = [prog_a[k] for k in xa]
            
            xb = sorted(prog_b.keys())
            yb = [prog_b[k] for k in xb]
            
            ax.plot(xa, ya, marker='o', label='Proc (A)', color='blue')
            ax.plot(xb, yb, marker='x', label='Thread (B)', color='red')
            
            if i == 0:
                ax.set_title(titles[j], fontsize=14, fontweight='bold')
            if j == 0:
                ax.set_ylabel(f"{task.upper()} Task\n{ylabels[j]}", fontsize=12, fontweight='bold')
            
            ax.grid(True)
            if i == 0 and j == 0:
                ax.legend()

    plt.suptitle('Part D: Comprehensive Scaling Analysis (Rows=Task Type, Cols=Metric)', fontsize=16)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig('MT25091_Part_D_Plot_1.png')
    print("Generated MT25091_Part_D_Plot_1.png with 12 subplots (3x4 grid)")

def plot_memory_efficiency(csv_file):
    """
    Generates a specific plot for Memory Efficiency (Mem Usage / N)
    """
    if not os.path.exists(csv_file):
        print(f"File {csv_file} not found.")
        return

    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f, skipinitialspace=True)
        # Skip potential garbage line if any, similar to plot_part_d logic
        # But actually reader handles it if structure is clean. 
        # Re-using simple read logic
        for row in reader:
            data.append(row)    

    plt.figure(figsize=(10, 6))
    
    # Filter for Memory Task only
    task = "mem"
    
    proc_x = []
    proc_y = []
    thread_x = []
    thread_y = []

    # Process Data
    for row in data:
        conf = row['Configuration']
        if 'Program_A' in conf and f'_{task}_' in conf:
            n = int(conf.split('_')[-1])
            mem = float(row['Mem_Usage'])
            proc_x.append(n)
            proc_y.append(mem / n) # Per Worker
    
    # Thread Data
    for row in data:
        conf = row['Configuration']
        if 'Program_B' in conf and f'_{task}_' in conf:
            n = int(conf.split('_')[-1])
            mem = float(row['Mem_Usage'])
            thread_x.append(n)
            thread_y.append(mem / n) # Per Worker
            
    plt.plot(proc_x, proc_y, marker='o', label='Process (A)', linestyle='-', color='blue')
    plt.plot(thread_x, thread_y, marker='x', label='Thread (B)', linestyle='--', color='red')
    
    plt.title("Per-Worker Memory Overhead (Efficiency)")
    plt.xlabel("Number of Workers (N)")
    plt.ylabel("Memory Usage per Worker (%)")
    plt.legend()
    plt.grid(True)
    
    plt.legend()
    plt.grid(True)
    
    plt.savefig('MT25091_Part_D_Plot_2.png')
    print("Generated MT25091_Part_D_Plot_2.png")

if __name__ == "__main__":
    plot_part_c("MT25091_Part_C_CSV.csv")
    plot_part_d("MT25091_Part_D_CSV.csv")
    plot_memory_efficiency("MT25091_Part_D_CSV.csv")
