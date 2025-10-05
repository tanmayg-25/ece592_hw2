import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

def plot_inclusivity(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found.")
        return
    
    print("=" * 60)
    print("LLC INCLUSIVITY TEST RESULTS")
    print("=" * 60)
    
    # Store raw data for plotting
    initial_raw = data['initial_hit_time'].copy()
    probe_raw = data['probe_after_evict_time'].copy()
    
    # Filter outliers INDEPENDENTLY for each column
    def filter_outliers(series, sigma=2.5):
        mean = series.mean()
        std = series.std()
        if std == 0:
            return series
        lower = mean - sigma * std
        upper = mean + sigma * std
        return series[(series >= lower) & (series <= upper)]
    
    initial_filtered = filter_outliers(initial_raw)
    probe_filtered = filter_outliers(probe_raw)
    
    # Calculate statistics
    initial_mean = initial_filtered.mean()
    initial_median = initial_filtered.median()
    initial_std = initial_filtered.std()
    
    probe_mean = probe_filtered.mean()
    probe_median = probe_filtered.median()
    probe_std = probe_filtered.std()
    
    ratio = probe_mean / initial_mean if initial_mean > 0 else 0
    
    print(f"\nRaw data points: {len(data)}")
    print(f"After filtering: Initial={len(initial_filtered)}, Probe={len(probe_filtered)}")
    
    print("\n--- INITIAL L1 HIT TIME ---")
    print(f"  Mean:   {initial_mean:.2f} cycles")
    print(f"  Median: {initial_median:.2f} cycles")
    print(f"  StdDev: {initial_std:.2f} cycles")
    
    print("\n--- PROBE AFTER L3 EVICTION ---")
    print(f"  Mean:   {probe_mean:.2f} cycles")
    print(f"  Median: {probe_median:.2f} cycles")
    print(f"  StdDev: {probe_std:.2f} cycles")
    
    print(f"\n--- ANALYSIS ---")
    print(f"  Latency Increase: {ratio:.2f}x")
    
    # Determine inclusivity
    threshold = 2.0
    if ratio > threshold:
        conclusion = "INCLUSIVE"
        explanation = (
            f"The probe latency is {ratio:.1f}× slower after L3 eviction.\n"
            "This indicates that evicting L3 also invalidated the data in L1/L2,\n"
            "forcing a memory access. This is characteristic of INCLUSIVE caches."
        )
    else:
        conclusion = "NON-INCLUSIVE or EXCLUSIVE"
        explanation = (
            f"The probe latency is only {ratio:.1f}× the initial hit time.\n"
            "The data remained in L1 despite L3 eviction, indicating that\n"
            "L1/L2 and L3 operate independently (non-inclusive or exclusive)."
        )
    
    print(f"\n{'='*60}")
    print(f"CONCLUSION: Cache is {conclusion}")
    print(f"{'='*60}")
    print(explanation)
    print("=" * 60)
    
    # --- Create visualization ---
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Left plot: Histogram with RAW data (no filtering for visualization)
    bins_initial = np.linspace(0, min(200, initial_raw.quantile(0.99)), 50)
    bins_probe = np.linspace(0, probe_raw.quantile(0.99), 50)
    
    ax1.hist(initial_raw, bins=bins_initial, alpha=0.7, 
             label=f'Initial L1 Hit\n(mean={initial_mean:.1f} cycles)', 
             color='skyblue', edgecolor='black')
    ax1.hist(probe_raw, bins=bins_probe, alpha=0.7, 
             label=f'Probe after L3 Eviction\n(mean={probe_mean:.1f} cycles)', 
             color='salmon', edgecolor='black')
    
    ax1.axvline(initial_mean, color='blue', linestyle='--', linewidth=2, label=f'L1 mean')
    ax1.axvline(probe_mean, color='red', linestyle='--', linewidth=2, label=f'Probe mean')
    
    ax1.set_xlabel("Latency (CPU Cycles)", fontsize=12, weight='bold')
    ax1.set_ylabel("Frequency (Count)", fontsize=12, weight='bold')
    ax1.set_title("Distribution Comparison", fontsize=14, weight='bold')
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    
    # Right plot: Box plot for clear comparison
    box_data = [initial_filtered.values, probe_filtered.values]
    bp = ax2.boxplot(box_data, labels=['L1 Hit', 'Post-Eviction'], 
                     patch_artist=True, widths=0.6)
    
    bp['boxes'][0].set_facecolor('skyblue')
    bp['boxes'][1].set_facecolor('salmon')
    
    for element in ['whiskers', 'fliers', 'means', 'medians', 'caps']:
        plt.setp(bp[element], color='black', linewidth=1.5)
    
    ax2.set_ylabel("Latency (CPU Cycles)", fontsize=12, weight='bold')
    ax2.set_title("Box Plot Comparison", fontsize=14, weight='bold')
    ax2.grid(True, alpha=0.3, axis='y')
    
    # Add conclusion box
    conclusion_text = f"CONCLUSION: Cache is {conclusion}\nLatency increase: {ratio:.1f}×"
    fig.text(0.5, 0.02, conclusion_text, ha='center', fontsize=14, weight='bold',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8, pad=0.8))
    
    plt.suptitle("LLC Inclusivity Test Results (Artemisia)", fontsize=16, weight='bold', y=0.98)
    plt.tight_layout(rect=[0, 0.05, 1, 0.96])
    
    output_filename = 'inclusivity_analysis.png'
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')
    print(f"\n✓ Plot saved as '{output_filename}'")
    plt.close()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
        print(f"Example: python {sys.argv[0]} inclusivity_data.csv")
    else:
        plot_inclusivity(sys.argv[1])