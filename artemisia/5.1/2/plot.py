import sys
import os
import pandas as pd
import matplotlib.pyplot as plt

# --- helper functions ---
def parse_size(size_str):
    """Convert Linux sysfs size string to bytes (e.g. '48K' -> 49152)."""
    size_str = size_str.strip().upper()
    if size_str.endswith("K"):
        return int(size_str[:-1]) * 1024
    elif size_str.endswith("M"):
        return int(size_str[:-1]) * 1024 * 1024
    return int(size_str)

def get_cache_sizes():
    """Read cache sizes from sysfs (/sys/devices/system/cpu/cpu0/cache)."""
    cache_sizes = {}
    try:
        base_path = "/sys/devices/system/cpu/cpu0/cache/"
        for i in range(10):
            index_path = os.path.join(base_path, f"index{i}")
            if not os.path.exists(index_path):
                break
            with open(os.path.join(index_path, "level")) as f:
                level = int(f.read().strip())
            with open(os.path.join(index_path, "type")) as f:
                cache_type = f.read().strip()
            with open(os.path.join(index_path, "size")) as f:
                size_str = f.read().strip()

            size_bytes = parse_size(size_str)
            if level == 1 and cache_type == "Data":
                cache_sizes["L1"] = size_bytes
            elif level == 2 and cache_type == "Unified":
                cache_sizes["L2"] = size_bytes
            elif level == 3 and cache_type == "Unified":
                cache_sizes["L3"] = size_bytes
    except Exception as e:
        print(f"Could not detect cache sizes: {e}")
    return cache_sizes

# --- main plotting ---
def plot_cache(csv_file):
    try:
        data = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"Error: file '{csv_file}' not found")
        return

    # sort by working set
    data = data.sort_values("working_set_size_bytes")

    # --- noise removal: keep mean ± 1 std ---
    filtered_data = []
    for size, group in data.groupby("working_set_size_bytes"):
        if len(group) < 2:
            filtered_data.append(group)
            continue
        mean = group["time_per_access_cycles"].mean()
        std = group["time_per_access_cycles"].std()
        cleaned = group[
            (group["time_per_access_cycles"] >= mean - std)
            & (group["time_per_access_cycles"] <= mean + std)
        ]
        filtered_data.append(cleaned)
    cleaned_df = pd.concat(filtered_data, ignore_index=True)

    # summary (mean/std)
    summary = cleaned_df.groupby("working_set_size_bytes")[
        "time_per_access_cycles"
    ].agg(["mean", "std"]).reset_index()

    # --- plot ---
    plt.figure(figsize=(10, 6))
    plt.plot(
        summary["working_set_size_bytes"],
        summary["mean"],
        linestyle="-",
        color="teal",
        label="Mean latency (±1σ)"
    )
    plt.fill_between(
        summary["working_set_size_bytes"],
        summary["mean"] - summary["std"],
        summary["mean"] + summary["std"],
        color="teal",
        alpha=0.2
    )

    plt.xscale("log", base=2)
    plt.xlabel("Working Set Size (bytes, log2)")
    plt.ylabel("Time per Access (cycles)")
    plt.title("Cache Hierarchy Latency (Artemisia)")
    plt.grid(True, which="both", linestyle="--", alpha=0.7)

    # add cache levels
    cache_sizes = get_cache_sizes()
    for name, size in cache_sizes.items():
        if size <= summary["working_set_size_bytes"].max():
            plt.axvline(size, color="darkviolet", linestyle="--", alpha=0.7)
            plt.text(size * 1.05, plt.ylim()[1] * 0.9, name,
                     rotation=90, color="darkviolet",
                     fontsize=11, fontweight="bold")

    plt.legend()
    plt.tight_layout()

    output_file = "cache_hierarchy_plot.png"
    plt.savefig(output_file, dpi=300)
    print(f"Plot saved as {output_file}")

# --- entrypoint ---
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <csv_file>")
        sys.exit(1)
    plot_cache(sys.argv[1])
