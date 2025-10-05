import pandas as pd
import matplotlib.pyplot as plt
import sys

def create_bargraph(csv_filename):

    # --- 1. Load the Data ---
    try:
        data = pd.read_csv(csv_filename)
        if 'hit_time' not in data.columns or 'miss_time' not in data.columns:
            print(f"Error: CSV file must contain 'hit_time' and 'miss_time' columns.")
            return
    except FileNotFoundError:
        print(f"Error: The file '{csv_filename}' was not found.")
        return

    # --- 2. Calculate Statistics and Filtering Bounds (Mean +/- 2 SD) ---
    # We use these bounds for the detailed histogram.
    hit_mean = data['hit_time'].mean()
    hit_std = data['hit_time'].std()
    hit_lower_bound = hit_mean - (hit_std)
    hit_upper_bound = hit_mean + (hit_std)

    miss_mean = data['miss_time'].mean()
    miss_std = data['miss_time'].std()
    miss_lower_bound = miss_mean - (miss_std)
    miss_upper_bound = miss_mean + (miss_std)
    
    # Apply the filters to get the data for the histogram
    filtered_hits = data[(data['hit_time'] >= hit_lower_bound) & (data['hit_time'] <= hit_upper_bound)]
    filtered_misses = data[(data['miss_time'] >= miss_lower_bound) & (data['miss_time'] <= miss_upper_bound)]
    
    print("------------- Average Latency ---------------")
    print(f"Mean L3 Hit Time:  {hit_mean:.2f} cycles")
    print(f"Mean L3 Miss Time: {miss_mean:.2f} cycles")
    print("-" * 45)

    # --- 3. Create the Combined Plot ---
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # --- Plot 1: Histogram on the left subplot (ax1) ---
    ax1.hist(filtered_hits['hit_time'], bins=50, alpha=0.7, label='L3 Hits')
    ax1.hist(filtered_misses['miss_time'], bins=50, alpha=0.7, label='L3 Misses')
    ax1.set_xlabel('Time (Cycles)')
    ax1.set_ylabel('Number of Observations')
    ax1.set_title('LLC Cache Hits VS Main Memory Access Latency')
    ax1.legend()
    ax1.grid(True, linestyle='--')

    # --- Plot 2: Summary Bar Chart on the right subplot (ax2) ---
    labels = ['L3 Hit', 'L3 Miss']
    means = [hit_mean, miss_mean] 
    
    bars = ax2.bar(labels, means, color=['skyblue', 'salmon'], width=0.5)
    ax2.set_ylabel('Average Time (Cycles)')
    ax2.set_title('Summary: Average Latency')
    ax2.grid(True, axis='y', linestyle='--', alpha=0.7)
    
    # Add the mean value as text on top of each bar
    for bar in bars:
        yval = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2.0, yval, f'{yval:.2f}', va='bottom', ha='center', fontsize=12)
    
    # --- 4. Save and Show the Final Figure ---
    plt.tight_layout(rect=[0, 0.03, 1, 0.95]) # Adjust layout to make room for suptitle
    output_filename = 'exercise1_graphs.png'
    plt.savefig(output_filename, dpi=300)
    print(f"Combined analysis plot saved as '{output_filename}'")
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python plot_combined_analysis.py <path_to_csv_file>")
    else:
        create_bargraph(sys.argv[1])