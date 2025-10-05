import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

def filter_outliers(series, sigma=2.5):
    """Remove outliers beyond sigma standard deviations from mean."""
    mean = series.mean()
    std = series.std()
    if std == 0:
        return series
    lower = mean - sigma * std
    upper = mean + sigma * std
    return series[(series >= lower) & (series <= upper)]

def plot_cache_latency(csv_filename):
    try:
        df = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found.")
        return

    print("=" * 70)
    print("CACHE LATENCY MEASUREMENT RESULTS")
    print("=" * 70)
    print(f"\nTotal samples: {len(df)}")

    # Store raw data
    l1_raw = df['l1_hit'].copy()
    l2_raw = df['l2_hit'].copy()
    l3_raw = df['l3_hit'].copy()
    ram_raw = df['ram_access'].copy()

    # Filter outliers independently for each measurement
    l1_filtered = filter_outliers(l1_raw)
    l2_filtered = filter_outliers(l2_raw)
    l3_filtered = filter_outliers(l3_raw)
    ram_filtered = filter_outliers(ram_raw)

    # Calculate statistics
    measurements = {
        'L1 Hit': (l1_filtered, l1_raw),
        'L2 Hit (L1 miss)': (l2_filtered, l2_raw),
        'L3 Hit (L1+L2 miss)': (l3_filtered, l3_raw),
        'RAM Access (all miss)': (ram_filtered, ram_raw)
    }

    stats = {}
    print("\n" + "-" * 70)
    for name, (filtered, raw) in measurements.items():
        mean = filtered.mean()
        median = filtered.median()
        std = filtered.std()
        p25 = filtered.quantile(0.25)
        p75 = filtered.quantile(0.75)
        
        stats[name] = {
            'mean': mean,
            'median': median,
            'std': std,
            'p25': p25,
            'p75': p75,
            'filtered': filtered,
            'raw': raw
        }
        
        print(f"{name:25s}: {mean:7.2f} cycles (median={median:.2f}, std={std:.2f})")
        print(f"{'':25s}  [Q1={p25:.2f}, Q3={p75:.2f}]")
        print(f"{'':25s}  Samples after filtering: {len(filtered)}/{len(raw)}")
        print()

    print("-" * 70)

    # Calculate latency differences
    l2_penalty = stats['L2 Hit (L1 miss)']['mean'] - stats['L1 Hit']['mean']
    l3_penalty = stats['L3 Hit (L1+L2 miss)']['mean'] - stats['L2 Hit (L1 miss)']['mean']
    ram_penalty = stats['RAM Access (all miss)']['mean'] - stats['L3 Hit (L1+L2 miss)']['mean']

    print("\n--- LATENCY PENALTIES ---")
    print(f"L1 miss penalty (L2 hit):          +{l2_penalty:.2f} cycles")
    print(f"L2 miss penalty (L3 hit):          +{l3_penalty:.2f} cycles")
    print(f"L3 miss penalty (RAM access):      +{ram_penalty:.2f} cycles")
    print(f"Total miss penalty (L1 → RAM):     +{ram_penalty + l3_penalty + l2_penalty:.2f} cycles")
    print("=" * 70)

    # Create visualization
    plt.figure(figsize=(16, 10))
    
    # Top: Histogram with all distributions
    
    colors = ['#2ecc71', '#3498db', '#f39c12', '#e74c3c']
    labels = list(measurements.keys())
    
    for i, (name, (filtered, raw)) in enumerate(measurements.items()):
        # Use raw data for histogram, but show filtered mean
        bins = np.linspace(0, raw.quantile(0.99), 60)
        plt.hist(raw, bins=bins, alpha=0.6, color=colors[i], 
                label=f'{name}\n(mean={stats[name]["mean"]:.1f} cycles)',
                edgecolor='black', linewidth=0.5)
        
        # Add vertical line for mean
        plt.axvline(stats[name]['mean'], color=colors[i], 
                   linestyle='--', linewidth=2, alpha=0.8)
    
    plt.xlabel("Latency (CPU Cycles)", fontsize=12, weight='bold')
    plt.ylabel("Frequency (Count)", fontsize=12, weight='bold')
    plt.title("Cache Hierarchy Latency Distribution (Artemisia)", fontsize=14, weight='bold')
    plt.legend(loc='upper right', fontsize=10)
    plt.grid(True, alpha=0.3)
    
    output_filename = 'cache_latency_analysis.png'
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')
    print(f"\n Plot saved as '{output_filename}'")
    


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
        print(f"Example: python {sys.argv[0]} cache_latency_data.csv")
    else:
        plot_cache_latency(sys.argv[1])