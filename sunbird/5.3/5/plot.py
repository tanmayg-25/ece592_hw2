import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.colors as colors
import numpy as np
import sys

def plot_cache_heatmap(csv_filename):
    try:
        data = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found.")
        return

    print("=" * 70)
    print("CACHE HIERARCHY HEATMAP ANALYSIS")
    print("=" * 70)
    print(f"Total data points: {len(data)}")
    print(f"Array sizes: {data['array_size_kb'].min()} KB to {data['array_size_kb'].max()} KB")
    print(f"Strides: {data['stride_bytes'].min()} to {data['stride_bytes'].max()} bytes")
    print()

    # Pivot data for heatmap
    heatmap_data = data.pivot_table(
        index='array_size_kb',
        columns='stride_bytes',
        values='avg_cycles_per_access'
    )

    print("Average cycles per access range:")
    print(f"  Minimum: {data['avg_cycles_per_access'].min():.2f}")
    print(f"  Maximum: {data['avg_cycles_per_access'].max():.2f}")
    print()

    # Identify cache level transitions (approximate)
    stride_64 = data[data['stride_bytes'] == 64].copy().sort_values('array_size_kb')
    latencies = stride_64['avg_cycles_per_access'].values
    sizes = stride_64['array_size_kb'].values

    print("Cache level transitions (stride=64 bytes):")
    for i in range(1, len(latencies)):
        ratio = latencies[i] / latencies[i-1]
        if ratio > 1.3:  # Detect jumps
            print(f"  {sizes[i-1]:6d} KB -> {sizes[i]:6d} KB: "
                  f"{latencies[i-1]:6.2f} -> {latencies[i]:6.2f} cycles "
                  f"({ratio:.2f}×)")
    print()

    # Create figure
    fig, ax = plt.subplots(figsize=(12, 6))

    # Draw heatmap
    im = ax.imshow(
        heatmap_data,
        cmap='plasma',
        norm=colors.LogNorm(vmin=heatmap_data.min().min(),
                            vmax=heatmap_data.max().max()),
        aspect='auto',
        interpolation='nearest'
    )

    ax.set_title('Memory Access Latency: Array Size vs. Stride (Sunbird)',
                 fontsize=14, weight='bold', pad=15)

    # Y-axis (Array Size)
    y_ticks = np.arange(len(heatmap_data.index))
    y_labels = [f"{int(s)}" for s in heatmap_data.index]
    ax.set_yticks(y_ticks[::2])
    ax.set_yticklabels(y_labels[::2])
    ax.set_ylabel('Array Size (KB)', fontsize=12, weight='bold')

    # X-axis (Stride)
    x_ticks = np.arange(len(heatmap_data.columns))
    x_labels = [f"{int(s)}" for s in heatmap_data.columns]
    ax.set_xticks(x_ticks)
    ax.set_xticklabels(x_labels, rotation=45)
    ax.set_xlabel('Stride (Bytes)', fontsize=12, weight='bold')

    # Colorbar
    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label('Avg Cycles/Access (Log Scale)', fontsize=11, weight='bold')

    # Cache level annotations (based on your per-core info)
    cache_levels = [
        (32, 'L1: 32 KB', 'yellow'),
        (256, 'L2: 256 KB', 'orange'),
        (30*1024, 'L3: ~30 MB', 'red')
    ]

    for size_kb, label, color in cache_levels:
        if size_kb >= heatmap_data.index.min() and size_kb <= heatmap_data.index.max():
            idx = np.argmin(np.abs(heatmap_data.index - size_kb))
            ax.axhline(y=idx, color=color, linestyle='--', linewidth=2, alpha=0.7)
            ax.text(len(heatmap_data.columns) + 0.5, idx, label,
                    color=color, fontsize=10, weight='bold', va='center')

    plt.tight_layout()

    output_filename = 'cache_hierarchy_heatmap.png'
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')

    print("=" * 70)
    print(f"Heatmap saved as '{output_filename}'")
    print("=" * 70)
    print("\nInterpretation Guide:")
    print("- Horizontal bands (plateaus): Cache level transitions")
    print("- Dark (low cycles): Data fits in faster cache")
    print("- Bright (high cycles): Data exceeds cache, slower access")
    print("- Vertical patterns: Stride effects on cache line utilization")
    print("- Large strides = poor spatial locality = more cache misses")
    print("=" * 70)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
        print(f"Example: python {sys.argv[0]} cache_heatmap.csv")
    else:
        plot_cache_heatmap(sys.argv[1])
