import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_branch_prediction(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found.")
        return

    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(figsize=(10, 7))

    # Create the bar chart
    colors = ['#4CAF50', '#81C784', '#FF7043']
    bars = ax.bar(data['test_type'], data['cycles_per_iteration'], color=colors)

    # Formatting
    ax.set_ylabel('Average Cycles per Iteration', fontsize=12)
    ax.set_title('Performance of Different Branch Patterns (Artemisia)', fontsize=16, weight='bold')
    ax.set_xticklabels(['Always Taken', 'Never Taken', 'Unpredictable'], rotation=0)
    ax.grid(axis='y', linestyle='--')

    # Add text labels on top of each bar
    for bar in bars:
        yval = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2.0, yval, f'{yval:.2f}', va='bottom', ha='center')

    output_filename = 'branch_prediction_analysis.png'
    plt.tight_layout()
    plt.savefig(output_filename, dpi=300)
    print(f"\nAnalysis plot saved as '{output_filename}'")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
    else:
        plot_branch_prediction(sys.argv[1])
