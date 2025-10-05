import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_pipeline_results(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: file '{csv_filename}' not found")
        return

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
    
    colors = {'independent': '#26C6DA', 'dependent': '#FF7043'}
    labels = {'independent': 'Independent (Throughput)', 'dependent': 'Dependent (Latency)'}
    
    # Reorder for plotting
    data['test_type'] = pd.Categorical(data['test_type'], categories=['independent', 'dependent'], ordered=True)
    data = data.sort_values('test_type')

    # --- Plot 1: CPI (Lower is better) ---
    bars1 = ax1.bar(data['test_type'], data['cpi'], color=[colors[t] for t in data['test_type']])
    ax1.set_ylabel('Cycles Per Instruction (CPI)')
    ax1.set_title('CPI: Throughput vs. Latency')
    ax1.set_xticklabels([labels[t] for t in data['test_type']])
    for bar in bars1:
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height, f'{height:.2f}', ha='center', va='bottom')

    # --- Plot 2: IPC (Higher is better) ---
    bars2 = ax2.bar(data['test_type'], data['ipc'], color=[colors[t] for t in data['test_type']])
    ax2.set_ylabel('Instructions Per Cycle (IPC)')
    ax2.set_title('IPC: Throughput vs. Latency')
    ax2.set_xticklabels([labels[t] for t in data['test_type']])
    for bar in bars2:
        height = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., height, f'{height:.2f}', ha='center', va='bottom')

    fig.suptitle('Pipelining & Superscalar Analysis', fontsize=16, weight='bold')
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig('pipeline_analysis.png', dpi=300)
    print("\nPlot saved as pipeline_analysis.png")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
    else:
        plot_pipeline_results(sys.argv[1])
