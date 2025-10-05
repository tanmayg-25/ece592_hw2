import pandas as pd
import matplotlib.pyplot as plt
import sys
import seaborn as sns # Seaborn is excellent for density plots

def create_plot_from_sweep(csv_filename):
    # --- 1. Load the Data ---
    try:
        data = pd.read_csv(csv_filename)
        if 'working_set_size_bytes' not in data.columns or 'time_per_access_cycles' not in data.columns:
            print("Error: CSV must contain 'working_set_size_bytes' and 'time_per_access_cycles'.")
            return
    except FileNotFoundError:
        print(f"Error: The file '{csv_filename}' was not found.")
        return

    # --- 2. Identify "Hit" and "Miss" Regions from the Sweep Data ---
    hit_data_raw = data[data['working_set_size_bytes'] <= 32 * 1024]['time_per_access_cycles']
    miss_data_raw = data[data['working_set_size_bytes'] >= 64 * 1024 * 1024]['time_per_access_cycles']

    if hit_data_raw.empty or miss_data_raw.empty:
        print("Error: Could not find data in the defined hit/miss size ranges.")
        print("Please ensure your sweep covers sizes from at least 4KB to 64MB.")
        return

    # --- 3. Filter the Hit and Miss Data (Mean +/- 2 SD) ---
    hit_mean = hit_data_raw.mean()
    hit_std = hit_data_raw.std()
    hit_data_filtered = hit_data_raw[(hit_data_raw >= hit_mean - (2 * hit_std)) & (hit_data_raw <= hit_mean + (2 * hit_std))]

    miss_mean = miss_data_raw.mean()
    miss_std = miss_data_raw.std()
    miss_data_filtered = miss_data_raw[(miss_data_raw >= miss_mean - (2 * miss_std)) & (miss_data_raw <= miss_mean + (2 * miss_std))]

    print("------------- Average Latency (from filtered data) ---------------")
    print(f"Mean 'Cache Hit' Time (<=32KB): {hit_data_filtered.mean():.2f} cycles")
    print(f"Mean 'Cache Miss' Time (>=64MB): {miss_data_filtered.mean():.2f} cycles")
    print("-" * 65)

   # --- 4. Create the Line Graph (KDE Plot) ---
    plt.style.use('seaborn-v0_8-whitegrid')
    plt.figure(figsize=(12, 7))

    # Convert to numeric arrays for seaborn
    hit_array = pd.to_numeric(hit_data_filtered, errors="coerce").dropna().to_numpy()
    miss_array = pd.to_numeric(miss_data_filtered, errors="coerce").dropna().to_numpy()

    # Create density plots
    sns.kdeplot(x=miss_array, color='salmon', fill=True, label='Cache Misses (>=64MB)')
    sns.kdeplot(x=hit_array, color='skyblue', fill=True, label='Cache Hits (<=32KB)')

    # Formatting the plot
    plt.title('Distribution of Hit vs. Miss Latency (Artemisia)', fontsize=16, weight='bold')
    plt.xlabel('Time (Cycles)', fontsize=12)
    plt.ylabel('Density (Frequency)', fontsize=12)
    plt.legend()
    plt.grid(True, linestyle='--')
    
    # --- 5. Save ---
    output_filename = 'cache_presence_line_plot.png'
    plt.savefig(output_filename, dpi=300)
    print(f"Analysis plot saved as '{output_filename}'")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <path_to_csv_file>")
    else:
        create_plot_from_sweep(sys.argv[1])

