#!/bin/bash
# This script compiles, runs, and plots the results for Exercise 1.

echo "--- Running All Steps for Exercise 1 ---"

# Step 1: Compile the C code
echo "Compiling exercise1.c..."
gcc -O0 -o exercise1 exercise1.c
if [ $? -ne 0 ]; then echo "Compilation failed!"; exit 1; fi

# Step 2: Run the executable to generate data
echo "Generating results.csv..."
./exercise1 > results.csv

# Step 3: Run the Python script to plot the data
echo "Plotting results with plot_combined_analysis.py..."
python3 plot.py results.csv

echo "--- Exercise 1 Complete! ---"
