import struct
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import os
import pandas as pd
import threading
import subprocess
import time

def read_binary_file(filepath):
    with open(filepath, "rb") as f:
        # Read the size of the vector
        size = struct.unpack('I', f.read(4))[0]
        # Read the vector data
        data = struct.unpack(f'{size}I', f.read(size * 4))
    return data

def remove_outliers_percentile(data, lower_percentile=0.00, upper_percentile=0.99):
    # Calculate lower and upper percentiles
    lower_bound = np.percentile(data, lower_percentile * 100)
    upper_bound = np.percentile(data, upper_percentile * 100)
    
    # Filter out outliers
    filtered_data = [x for x in data if lower_bound <= x <= upper_bound]
    
    return filtered_data

def bench_one(cache_size, profile_name, warmup_times, run_times):
    # NOTE: this needs to be edited for remote caching
    # excute the benchmark
    server_process = subprocess.Popen(f'./Build/Server -p {cache_size}', shell=True)
    time.sleep(5)
    os.system(f'./Build/YCSB_CPP --warmup {warmup_times} --run {run_times} --cache --profile {profile_name}')
    os.system(f'python3 ./script/EndServer.py')
    server_process.wait()


def bench(cache_sizes, profile_prefix, warmup_times, run_times):
    for idx, cache_size in enumerate(cache_sizes):
        print(f'Benching {idx + 1} / {len(cache_sizes)}, cache size: {cache_size}M')
        # NOTE: this needs to be edited for remote caching
        bench_one(cache_size, f'{profile_prefix}_cache{cache_size}M', warmup_times, run_times)
    

def analyze_latency(cache_sizes, profile_prefix):

    # NOTE: this two needs to be tuned for a proper visualization
    bin_width = 5
    y_lim = 20000

    files = [f'{profile_prefix}_cache{cache_size}M_vector.log' for cache_size in cache_sizes]
    labels = [f'{cache_size}M Cache' for cache_size in cache_sizes]
    data_sets = [read_binary_file(file) for file in files]
    filtered_data_sets = [remove_outliers_percentile(data) for data in data_sets]
    filtered_data_sets_np = [np.array(data, dtype=np.float64) for data in filtered_data_sets]

    sns.set(style="whitegrid")
    plt.figure(figsize=(12, 8))
    for i, data in enumerate(filtered_data_sets_np):
        sns.histplot(data, label=labels[i], binwidth=bin_width, alpha=0.5, kde=True, kde_kws={'bw_adjust': 0.1})
        # since we remove > 99th percentile as outliers, here we should plot the 100th percentile (max value) 
        percentile_99 = np.percentile(data, 100)
        plt.axvline(percentile_99, color='black', linestyle='dashed', linewidth=1)
        plt.text(percentile_99, 0.0001, f'{labels[i]} 99%: {percentile_99:.2f}', rotation=90, verticalalignment='bottom', fontsize=12)

    plt.ylim(0, y_lim)
    plt.xlabel('Latency', fontsize=14)
    plt.ylabel('Density', fontsize=14)
    plt.title(f'Density Curve of Latency Data, {profile_prefix}', fontsize=16)
    plt.legend(title='Cache Configuration', fontsize=12)
    plt.tight_layout()
    plt.savefig(f'{profile_prefix}_latency_density_curves.png')

    # plot latency-hitrate plot with ci
    result_files = [f'{profile_prefix}_cache{cache_size}M_tailLatency.log' for cache_size in cache_sizes]
    # each file has n lines of [p99, p95, p50, hitrate, cache_size]
    # we need to read the p99, p95, p50, hitrate and cache_size
    result_data = []
    for idx, file in enumerate(result_files):
        with open(file, 'r') as f:
            lines = f.readlines()
            for line in lines:
                p99, p95, p50, hitrate = line.split(' ')
                result_data.append([float(p99), float(p95), float(p50), float(hitrate), int(cache_sizes[idx])])
    result_data = np.array(result_data)
    df = pd.DataFrame(result_data, columns=['p99', 'p95', 'p50', 'hitrate', 'cache_size'])

    summary_df = df.groupby('cache_size').agg(
        hitrate_mean=('hitrate', 'mean'),
        p99_mean=('p99', 'mean'),
        p95_mean=('p95', 'mean'),
        p50_mean=('p50', 'mean'),
        p99_ci=('p99', lambda x: 2.576 * x.std() / np.sqrt(len(x))),
        p95_ci=('p95', lambda x: 2.576 * x.std() / np.sqrt(len(x))),
        p50_ci=('p50', lambda x: 2.576 * x.std() / np.sqrt(len(x)))
    ).reset_index()

    # Plotting using Seaborn
    plt.figure(figsize=(12, 8))

    # p99 plot
    sns.lineplot(x='hitrate_mean', y='p99_mean', data=summary_df, label='p99', marker='o')
    plt.fill_between(summary_df['hitrate_mean'], 
                    summary_df['p99_mean'] - summary_df['p99_ci'], 
                    summary_df['p99_mean'] + summary_df['p99_ci'], alpha=0.2)

    # p95 plot
    sns.lineplot(x='hitrate_mean', y='p95_mean', data=summary_df, label='p95', marker='o')
    plt.fill_between(summary_df['hitrate_mean'], 
                    summary_df['p95_mean'] - summary_df['p95_ci'], 
                    summary_df['p95_mean'] + summary_df['p95_ci'], alpha=0.2)

    # p50 plot
    sns.lineplot(x='hitrate_mean', y='p50_mean', data=summary_df, label='p50', marker='o')
    plt.fill_between(summary_df['hitrate_mean'], 
                    summary_df['p50_mean'] - summary_df['p50_ci'], 
                    summary_df['p50_mean'] + summary_df['p50_ci'], alpha=0.2)

    plt.xlabel('Mean Hitrate', fontsize=14)
    plt.ylabel('Latency', fontsize=14)
    plt.title('Latency vs Hitrate with 99% CI, ' + profile_prefix, fontsize=16)
    plt.legend()
    plt.tight_layout()
    plt.savefig(f'{profile_prefix}_latency_hitrate_plot.png')

if __name__ == '__main__':
    cache_sizes = [32, 64, 128, 256, 256+128, 512, 512+128, 512+256, 512+128+256, 1024, 2048]
    profile_prefix = './data/tmdb_10G_hotspot'
    warmup_times = 7
    run_times = 12
    bench(cache_sizes, profile_prefix, warmup_times, run_times)
    analyze_latency(cache_sizes, profile_prefix)
    