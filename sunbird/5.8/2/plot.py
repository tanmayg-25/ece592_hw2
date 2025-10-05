#!/usr/bin/env python3
"""
PRF Size Analysis and Plotting Script
Usage: python plot_prf.py prf_raw_data.csv
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def find_knee(x, y):
    """Find the knee point in the curve using second derivative"""
    # Smooth the data first
    window = 5
    y_smooth = pd.Series(y).rolling(window=window, center=True).mean()
    
    # Calculate first and second derivatives
    dy = np.gradient(y_smooth)
    d2y = np.gradient(dy)
    
    # Find where second derivative is maximum (steepest increase)
    knee_idx = np.argmax(d2y[10:]) + 10  # Skip first few noisy points
    
    return x[knee_idx], y_smooth.iloc[knee_idx]

def main():
    if len(sys.argv) != 2:
        print("Usage: python plot_prf.py <csv_file>")
        print("Example: python plot_prf.py prf_raw_data.csv")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    
    try:
        # Read the CSV file
        print(f"Reading data from {csv_file}...")
        df = pd.read_csv(csv_file)
        
        # Group by ICOUNT and calculate statistics
        grouped = df.groupby('ICOUNT')['CYCLES'].agg(['median', 'mean', 'std', 'min', 'max'])
        
        print(f"\nProcessed {len(df)} measurements")
        print(f"ICOUNT range: {grouped.index.min()} to {grouped.index.max()}")
        
        # Find the knee point
        knee_icount, knee_cycles = find_knee(grouped.index.values, grouped['median'].values)
        
        print(f"\n{'='*50}")
        print(f"PRF SIZE ESTIMATE: ~{int(knee_icount)} registers")
        print(f"Knee detected at ICOUNT={int(knee_icount)}, Cycles={knee_cycles:.2f}")
        print(f"{'='*50}\n")
        
        # Create the plot
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
        
        # Plot 1: Main curve with confidence interval
        ax1.plot(grouped.index, grouped['median'], 'b-', linewidth=2, label='Median')
        ax1.fill_between(grouped.index, 
                         grouped['median'] - grouped['std'], 
                         grouped['median'] + grouped['std'],
                         alpha=0.3, label='±1 Std Dev')
        
        # Mark the knee point
        ax1.axvline(x=knee_icount, color='r', linestyle='--', linewidth=2, label=f'Knee @ {int(knee_icount)}')
        ax1.plot(knee_icount, knee_cycles, 'ro', markersize=10)
        
        ax1.set_xlabel('ICOUNT (Instructions in Flight)', fontsize=12)
        ax1.set_ylabel('Cycles per Instruction', fontsize=12)
        ax1.set_title('Physical Register File (PRF) Size Measurement (Sunbird)', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)
        ax1.legend(fontsize=10)
        
        # Plot 2: Derivative to show rate of change
        dy = np.gradient(grouped['median'])
        ax2.plot(grouped.index, dy, 'g-', linewidth=2)
        ax2.axvline(x=knee_icount, color='r', linestyle='--', linewidth=2)
        ax2.set_xlabel('ICOUNT (Instructions in Flight)', fontsize=12)
        ax2.set_ylabel('Rate of Change (d(Cycles)/d(ICOUNT))', fontsize=12)
        ax2.set_title('Derivative - Shows Where Performance Degrades', fontsize=12)
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # Save the plot
        output_file = csv_file.replace('.csv', '_plot.png')
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Plot saved to: {output_file}")
        
        # Also save a summary statistics file
        summary_file = csv_file.replace('.csv', '_summary.txt')
        with open(summary_file, 'w') as f:
            f.write("PRF Size Measurement Summary\n")
            f.write("="*50 + "\n\n")
            f.write(f"Estimated PRF Size: ~{int(knee_icount)} registers\n")
            f.write(f"Knee Point: ICOUNT={int(knee_icount)}, Cycles={knee_cycles:.2f}\n\n")
            f.write("Reference Values:\n")
            f.write("  - Intel Haswell:        ~168 INT registers\n")
            f.write("  - Intel Sapphire Rapids: ~332 INT registers\n\n")
            f.write("Statistics by ICOUNT:\n")
            f.write("-"*50 + "\n")
            f.write(grouped.to_string())
        
        print(f"Summary saved to: {summary_file}")
        
        # Show the plot
        plt.show()
        
    except FileNotFoundError:
        print(f"Error: File '{csv_file}' not found!")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()