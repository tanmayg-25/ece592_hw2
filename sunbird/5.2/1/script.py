import sys
import pandas as pd
import matplotlib.pyplot as plt

def main():
    # --- 1. Argument Parsing and Data Loading ---
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <path_to_csv_file>")
        sys.exit(1)

    csv_filename = sys.argv[1]
    try:
        df = pd.read_csv(csv_filename)
        # The C code uses 'NA' for random stride, which pandas might read as NaN.
        # We can fill it with a placeholder if needed, e.g., df['stride'].fillna(0, inplace=True)
        print(f"Successfully loaded '{csv_filename}'.")
    except FileNotFoundError:
        print(f"Error: The file '{csv_filename}' was not found.")
        sys.exit(1)

    # --- 2. Separate Data by Type ---
    sequential_data = df[df['type'] == 'sequential']['cycles_per_access']
    random_data = df[df['type'] == 'random']['cycles_per_access']
    
    # For stride data, we need the average time for each stride value
    stride_df = df[df['type'] == 'stride'].copy()
    # Convert stride from element count to bytes (assuming long is 8 bytes)
    stride_df['stride_bytes'] = stride_df['stride'] * 8
    stride_means = stride_df.groupby('stride_bytes')['cycles_per_access'].mean()

    # --- 3. Create the Combined Plot (2 subplots) ---
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 7))

    # --- Plot 1: Histogram for Sequential vs. Random (Frequency vs. Cycles) ---
    ax1.hist(sequential_data, bins=20, alpha=0.7, label='Sequential Access', color='blue')
    ax1.hist(random_data, bins=30, alpha=0.7, label='Random Access', color='red')
    ax1.set_title('Sequential vs. Random Access Distribution', fontsize=16, weight='bold')
    ax1.set_xlabel('Average Cycles per Access', fontsize=12)
    ax1.set_ylabel('Frequency (Number of Runs)', fontsize=12)
    ax1.legend()

    # --- Plot 2: Line Plot for Strided Access ---
    ax2.plot(stride_means.index, stride_means.values, marker='o', linestyle='-', color='green')
    ax2.set_xscale('log', base=2) # Stride doubles, so log scale is best
    ax2.set_title('Impact of Stride on Access Time', fontsize=16, weight='bold')
    ax2.set_xlabel('Stride Size (Bytes) - Log Scale', fontsize=12)
    ax2.set_ylabel('Average Cycles per Access', fontsize=12)
    
    # Set x-axis ticks to be the actual stride values
    ax2.set_xticks(stride_means.index)
    ax2.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    plt.setp(ax2.get_xticklabels(), rotation=45, ha="right")

    # --- 4. Save and Finalize ---
    fig.suptitle('Analysis of Memory Access Pattern (Sunbird)', fontsize=20)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    
    output_filename = "memory_access_analysis.png"
    plt.savefig(output_filename, dpi=300)
    print(f"\nAnalysis plot saved as '{output_filename}'")


if __name__ == "__main__":
    main()