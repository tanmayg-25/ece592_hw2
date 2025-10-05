#!/usr/bin/env python3
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import savgol_filter

def find_knee(x, y, window=11):
    """
    Find the knee point using second derivative method.
    The knee is where the curve transitions from flat to linear growth.
    """
    # Smooth the data first
    if len(y) > window:
        y_smooth = savgol_filter(y, window, 3)
    else:
        y_smooth = y
    
    # Compute first and second derivatives
    dy = np.gradient(y_smooth)
    ddy = np.gradient(dy)
    
    # Find the point of maximum second derivative (the knee)
    # Ignore the first and last 10% of points to avoid edge effects
    start_idx = len(ddy) // 10
    end_idx = len(ddy) - len(ddy) // 10
    
    if end_idx <= start_idx:
        start_idx = 0
        end_idx = len(ddy)
    
    knee_idx = start_idx + np.argmax(np.abs(ddy[start_idx:end_idx]))
    
    return x[knee_idx], y_smooth, dy, ddy

def main():
    if len(sys.argv) < 2:
        print("Usage: python plot_rob.py <csvfile> [machine_name]")
        print("Example: python plot_rob.py robsize.csv Artemisia")
        sys.exit(1)
    
    csvfile = sys.argv[1]
    machine_name = sys.argv[2] if len(sys.argv) > 2 else "Unknown"
    
    # Read CSV
    df = pd.read_csv(csvfile)
    
    # Extract data
    filler_counts = df['filler_count'].values
    avg_cycles = df['avg_cycles'].values
    min_cycles = df['min_cycles'].values
    max_cycles = df['max_cycles'].values
    
    # Find the knee point
    knee_x, smoothed, dy, ddy = find_knee(filler_counts, avg_cycles)
    
    print("\n" + "="*60)
    print(f"ROB SIZE ANALYSIS - {machine_name}")
    print("="*60)
    print(f"\nDetected ROB Size (Knee Point): ~{int(knee_x)} entries")
    print(f"\nData points: {len(df)}")
    print(f"Filler count range: {filler_counts[0]} to {filler_counts[-1]}")
    print(f"Cycle range: {avg_cycles.min():.2f} to {avg_cycles.max():.2f}")
    
    # Architecture detection
    if 190 <= knee_x <= 200:
        arch = "Haswell (192 ROB entries)"
    elif 350 <= knee_x <= 360:
        arch = "Ice Lake/Sunny Cove (352 ROB entries)"
    elif 500 <= knee_x <= 520:
        arch = "Sapphire Rapids/Golden Cove (512 ROB entries)"
    else:
        arch = "Unknown Architecture"
    
    print(f"Likely Architecture: {arch}")
    print("="*60 + "\n")
    
    # Create figure with subplots
    plt.figsize=(16,12)
    # Plot 1: Main data with knee point
    plt.plot(filler_counts, avg_cycles, 'o-', alpha=0.6, 
             label='Average Cycles', color='blue', markersize=4)
    plt.plot(filler_counts, smoothed, '-', linewidth=2, 
             label='Smoothed', color='darkblue')
    plt.axvline(knee_x, color='red', linestyle='--', linewidth=2, 
                label=f'Knee at {int(knee_x)}')
    plt.fill_between(filler_counts, min_cycles, max_cycles, 
                     alpha=0.2, color='blue', label='Min-Max Range')
    plt.xlabel('Filler Instruction Count', fontsize=12)
    plt.ylabel('Cycles per Iteration', fontsize=12)
    plt.title('ROB Size Detection (Artemisia)', fontsize=13)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='upper left')
    
    # Add annotation for knee
    plt.annotate(f'ROB Size\n~{int(knee_x)} entries', 
                xy=(knee_x, smoothed[np.argmin(np.abs(filler_counts - knee_x))]),
                xytext=(knee_x + 50, smoothed[np.argmin(np.abs(filler_counts - knee_x))] + 5),
                arrowprops=dict(arrowstyle='->', color='red', lw=2),
                fontsize=11, fontweight='bold', color='red',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
    plt.tight_layout()
    
    # Save figure
    outname = f"rob_analysis_{machine_name.lower()}.png"
    plt.savefig(outname, dpi=300, bbox_inches='tight')
    print(f"Plot saved as {outname}")
    
    # Print statistics table
    print("\n--- SUMMARY STATISTICS ---")
    print(f"{'Metric':<30} {'Value':>15}")
    print("-" * 46)
    print(f"{'Min cycles (baseline)':<30} {avg_cycles.min():>15.2f}")
    print(f"{'Max cycles':<30} {avg_cycles.max():>15.2f}")
    print(f"{'Cycles at knee point':<30} {smoothed[np.argmin(np.abs(filler_counts - knee_x))]:>15.2f}")
    print(f"{'Increase factor':<30} {avg_cycles.max() / avg_cycles.min():>15.2f}x")
    print(f"{'Detected ROB size':<30} {int(knee_x):>15} entries")

if __name__ == "__main__":
    main()