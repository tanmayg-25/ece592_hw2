#!/usr/bin/env python3
import sys
import pandas as pd
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) < 2:
        print("Usage: python plot.py <csvfile>")
        sys.exit(1)

    csvfile = sys.argv[1]

    # Read CSV
    df = pd.read_csv(csvfile)

    # Compute rolling average to filter noise
    window_size = max(1, len(df)//20)  # ~5% of runs
    df["smoothed"] = df["latency_cycles"].rolling(window=window_size, center=True).mean()

    # Print summary
    print("\n--- RAW RESULTS (first 10 runs) ---")
    print(df.head(10)[["run", "latency_cycles"]])
    print("\n--- SUMMARY ---")
    print(f"Total runs: {len(df)}")
    print(f"Min latency: {df['latency_cycles'].min():.4f} cycles")
    print(f"Max latency: {df['latency_cycles'].max():.4f} cycles")
    print(f"Average latency: {df['latency_cycles'].mean():.4f} cycles")

    # Plot
    plt.figure(figsize=(9,5))
    plt.plot(df["run"], df["latency_cycles"], alpha=0.4, label="Raw latency", color="gray")
    plt.plot(df["run"], df["smoothed"], linewidth=2, label=f"Smoothed (window={window_size})", color="blue")

    plt.xlabel("Run")
    plt.ylabel("Latency (cycles)")
    plt.title("AVX2 vpxor Latency Benchmark (Artemisia)")
    plt.grid(True, linestyle="--", alpha=0.6)
    plt.legend()

    # Save image
    outname = "latency_plot.png"
    plt.savefig(outname, dpi=300, bbox_inches="tight")
    print(f"\nPlot saved as {outname}")

if __name__ == "__main__":
    main()
