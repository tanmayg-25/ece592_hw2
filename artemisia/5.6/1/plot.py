import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys

def plot_amx_sweep(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: file '{csv_filename}' not found")
        return

    # Create a 1x2 plot: one for INT8, one for BF16
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, axes = plt.subplots(1, 2, figsize=(20, 8), sharey=True)
    fig.suptitle('AMX TMUL Performance vs. Input Sparsity', fontsize=18, weight='bold')

    data_types = ['INT8', 'BF16']
    for i, data_type in enumerate(data_types):
        ax = axes[i]
        subset = data[data['data_type'] == data_type]
        
        # Use seaborn to automatically plot different lines for each tile shape
        sns.lineplot(data=subset, x='sparsity_percent', y='avg_cycles', 
                     hue='tile_shape', style='tile_shape',
                     marker='o', markersize=8, ax=ax, linewidth=2.5)
        
        ax.set_xlabel('Sparsity (Percentage of Zeros)', fontsize=12)
        ax.set_ylabel('Average Execution Time per TMUL (Cycles)', fontsize=12)
        ax.set_title(f'{data_type} Performance', fontsize=14)
        ax.grid(True, which="both", linestyle='--')
        ax.legend(title='Tile Shape')
        ax.set_xticks(range(0, 101, 10))

    output_filename = 'amx_combined_analysis.png'
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(output_filename, dpi=300)
    print(f"\nPlot saved as {output_filename}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
    else:
        plot_amx_sweep(sys.argv[1])

