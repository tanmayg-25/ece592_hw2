import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys

def analyze_dmp(csvfile):
    try:
        df = pd.read_csv(csvfile)
    except FileNotFoundError:
        print(f"Error: File '{csvfile}' not found.")
        sys.exit(1)

    # --- 1. Filter Outliers (Mean ± 2 Standard Deviations) ---
    print("Filtering outliers from raw data...")
    filtered_groups = []
    for pattern, group in df.groupby('pattern'):
        if len(group) < 2:
            filtered_groups.append(group)
            continue
        mean = group['cycles_per_step'].mean()
        std = group['cycles_per_step'].std()
        # Keep data within 2 standard deviations of the mean
        cleaned_group = group[abs(group['cycles_per_step'] - mean) <= 2 * std]
        filtered_groups.append(cleaned_group)
    
    cleaned_df = pd.concat(filtered_groups, ignore_index=True)
    print(f"Removed {len(df) - len(cleaned_df)} outlier points.")

    # --- 2. Calculate Final Averages ---
    summary = cleaned_df.groupby('pattern')['cycles_per_step'].mean().sort_values()
    
    print("\n--- Average Latency per Pattern (Filtered) ---")
    print(summary)
    print("----------------------------------------------")

    # --- 3. Create the Bar Chart ---
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(figsize=(12, 8))

    colors = sns.color_palette("viridis", n_colors=len(summary))
    bars = ax.bar(summary.index, summary.values, color=colors)

    # --- 4. Formatting and Saving ---
    ax.set_ylabel('Average Cycles per Access', fontsize=12)
    ax.set_title('Pointer-Chasing Latency for Different Memory Patterns (Artemisia)', fontsize=16, weight='bold')
    ax.grid(axis='y', linestyle='--')
    
    # Add text labels on top of each bar
    for bar in bars:
        yval = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2.0, yval, f'{yval:.1f}', va='bottom', ha='center')

    # Add an analysis note

    output_filename = 'dmp_analysis.png'
    plt.tight_layout()
    plt.savefig(output_filename, dpi=300)
    print(f"\nBar chart saved as {output_filename}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
        sys.exit(1)
    analyze_dmp(sys.argv[1])
