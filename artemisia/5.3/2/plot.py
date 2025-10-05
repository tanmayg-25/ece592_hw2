import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_cache_line_size_filtered(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found.")
        return

    # --- ADDED: Outlier Filtering (Mean ± 1 Standard Deviation) ---
    print("Filtering outliers for each stride group...")
    filtered_groups = []
    for stride, group in data.groupby('stride_bytes'):
        if len(group) < 2:
            filtered_groups.append(group)
            continue
        
        mean = group['avg_cycles_per_access'].mean()
        std = group['avg_cycles_per_access'].std()
        
        # Define the bounds
        lower_bound = mean - std
        upper_bound = mean + std

        # Keep only the data within the bounds
        cleaned_group = group[
            (group['avg_cycles_per_access'] >= lower_bound) & 
            (group['avg_cycles_per_access'] <= upper_bound)
        ]
        filtered_groups.append(cleaned_group)

    cleaned_df = pd.concat(filtered_groups, ignore_index=True)
    print(f"Removed {len(data) - len(cleaned_df)} outlier points.")

    # --- Summarize the cleaned data ---
    summary = cleaned_df.groupby('stride_bytes')['avg_cycles_per_access'].agg(['mean', 'std']).reset_index()

    # --- Plotting (now with error bars) ---
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(figsize=(12, 8))

    # Use errorbar to show the mean and standard deviation of the cleaned data
    ax.errorbar(summary['stride_bytes'], summary['mean'], yerr=summary['std'], 
                marker='o', linestyle='-', color='crimson', capsize=3,
                label='Mean Latency')

    ax.set_xscale('log', base=2)
    ax.set_xticks(summary['stride_bytes'])
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())

    # Formatting (unchanged)
    ax.set_xlabel('Stride Size (Bytes) - Log Scale', fontsize=12)
    ax.set_ylabel('Average Cycles per Access', fontsize=12)
    ax.set_title('Impact of Memory Access Stride on Latency (Artemisia)', fontsize=16, weight='bold')
    ax.grid(True, which="both", linestyle='--')
    ax.axvline(64, color='darkblue', linestyle='--', label='Expected Cache Line Size (64 Bytes)')
    ax.legend()

    output_filename = 'cache_line_analysis.png'
    plt.tight_layout()
    plt.savefig(output_filename, dpi=300)
    print(f"\nFiltered analysis plot saved as '{output_filename}'")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
    else:
        plot_cache_line_size_filtered(sys.argv[1])
