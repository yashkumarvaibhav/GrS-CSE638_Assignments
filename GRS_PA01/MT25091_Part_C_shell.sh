#!/bin/bash

# CPU Pinning Configuration (Default to 0)
CPU_SET=${1:-0}
if [ -z "$1" ]; then
    echo "No CPU set specified. Defaulting to CPU 0."
    echo "Usage: ./MT25091_Part_C_shell.sh <cpu_list>"
fi

# Output Files
CSV_FILE_C="MT25091_Part_C_CSV.csv"
CSV_FILE_D="MT25091_Part_D_CSV.csv"

# Function to run measurement
# Usage: run_measurement <program> <args> <program_name_alias> <output_csv>
run_measurement() {
    PROG=$1
    ARGS=$2
    ALIAS=$3
    OUT_CSV=$4
    
    echo "Running: $ALIAS ($PROG $ARGS) pinned to CPU(s) $CPU_SET"
    
    # Start the program in the background, pinned to CPU_SET
    taskset -c $CPU_SET ./$PROG $ARGS &
    PID=$!
    
    # Capture start time for duration
    START_TIME=$(date +%s.%N)
    
    # Arrays to store samples
    CPU_SAMPLES=()
    MEM_SAMPLES=()
    IO_SAMPLES=()
    
    # Monitor while process is running
    while kill -0 $PID 2>/dev/null; do
        # Capture top output for 2 iterations to get delta
        # We capture system-wide top for the specific program name
        # grep -c "top -" checks for headers. We want the second block.
        
        top -b -n 2 -d 0.2 -c -w 512 > top_output.tmp
        
        # Find the line number of the second iteration header
        SECOND_ITER_LINE=$(grep -n "top -" top_output.tmp | head -n 2 | tail -n 1 | cut -d: -f1)
        
        # Extract the second iteration and filter for our program
        # We grep for $PROG (the binary name)
        # Using awk to sum CPU ($9) and MEM ($10) columns
        
        STATS=$(tail -n +$SECOND_ITER_LINE top_output.tmp | grep "$PROG" | awk '{cpu+=$9; mem+=$10} END {print cpu, mem}')
        
        if [ ! -z "$STATS" ]; then
             CURr_CPU=$(echo "$STATS" | awk '{print $1}')
             CURr_MEM=$(echo "$STATS" | awk '{print $2}')
             
             # Handle empty results (if grep found nothing)
             if [ -z "$CURr_CPU" ]; then CURr_CPU=0; fi
             if [ -z "$CURr_MEM" ]; then CURr_MEM=0; fi
             
             CPU_SAMPLES+=($CURr_CPU)
             MEM_SAMPLES+=($CURr_MEM)
        fi
        
        rm -f top_output.tmp
        
        # iostat for IO (capturing disk utilization %util/tps)
        # Getting the max util across devices
        CURR_IO=$(iostat -dx 1 1 | tail -n +4 | awk '{print $14}' | sort -nr | head -n 1)
        if [ ! -z "$CURR_IO" ]; then
             IO_SAMPLES+=($CURR_IO)
        else
             IO_SAMPLES+=(0)
        fi
        
        sleep 0.1
    done
    
    # Capture end time
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc)
    
    # Calculate Averages
    AVG_CPU=0
    AVG_MEM=0
    AVG_IO=0
    SAMPLE_COUNT=${#CPU_SAMPLES[@]}
    
    if [ $SAMPLE_COUNT -gt 0 ]; then
        TOTAL_CPU=0
        for i in "${CPU_SAMPLES[@]}"; do
            TOTAL_CPU=$(echo "$TOTAL_CPU + $i" | bc)
        done
        AVG_CPU=$(echo "scale=2; $TOTAL_CPU / $SAMPLE_COUNT" | bc)
        
        TOTAL_MEM=0
        for i in "${MEM_SAMPLES[@]}"; do
             TOTAL_MEM=$(echo "$TOTAL_MEM + $i" | bc)
        done
        AVG_MEM=$(echo "scale=2; $TOTAL_MEM / $SAMPLE_COUNT" | bc)
        
        TOTAL_IO=0
        COUNT_IO=${#IO_SAMPLES[@]}
        if [ $COUNT_IO -gt 0 ]; then
            for i in "${IO_SAMPLES[@]}"; do
                TOTAL_IO=$(echo "$TOTAL_IO + $i" | bc)
            done
            AVG_IO=$(echo "scale=2; $TOTAL_IO / $COUNT_IO" | bc)
        fi
    fi

    echo "$ALIAS    AVG_CPU: $AVG_CPU%    AVG_MEM: $AVG_MEM%    AVG_IO_Util: $AVG_IO%    TIME: $DURATION s"
    
    # Append to CSV
    echo "$ALIAS, $AVG_CPU, $AVG_MEM, $AVG_IO, $DURATION" >> $OUT_CSV
    echo "------------------------------------------------"
}

# --- Part C Execution ---
echo "Starting Part C Measurements..."
echo "Program_Variant, CPU_Usage, Mem_Usage, IO_Usage, Duration" > $CSV_FILE_C

PROGRAMS=("MT25091_Part_A_Program_A" "MT25091_Part_A_Program_B")
TASKS=("cpu" "mem" "io")

for PROG in "${PROGRAMS[@]}"; do
    NAME="Program_A"
    if [[ "$PROG" == *"Program_B"* ]]; then
        NAME="Program_B"
    fi
    
    for TASK in "${TASKS[@]}"; do
        run_measurement $PROG $TASK "${NAME}+${TASK}" $CSV_FILE_C
    done
done

# --- Part D Execution (Scaling) ---
echo "Starting Part D Measurements (Scaling)..."
echo "Configuration, CPU_Usage, Mem_Usage, IO_Usage, Duration" > $CSV_FILE_D

# Scale Processes (Program A)
for COUNT in 2 3 4 5; do
    for TASK in "${TASKS[@]}"; do
         run_measurement "MT25091_Part_A_Program_A" "$TASK $COUNT" "Program_A_Scaling_${TASK}_${COUNT}" $CSV_FILE_D
    done
done

# Scale Threads (Program B)
for COUNT in 2 3 4 5 6 7 8; do
    for TASK in "${TASKS[@]}"; do
         run_measurement "MT25091_Part_A_Program_B" "$TASK $COUNT" "Program_B_Scaling_${TASK}_${COUNT}" $CSV_FILE_D
    done
done

echo "All measurements complete."

# Generate Plots
echo "Generating plots..."
python3 MT25091_Part_D_Plot.py

if [ $? -eq 0 ]; then
    echo "Plots generated successfully."
else
    echo "Plot generation failed. Ensure python3 and matplotlib are installed."
fi
