import pandas as pd
import numpy as np

def read_files(cache_sizes, profile_prefix):
    result_files = [f'{profile_prefix}_cache{cache_size}M_tailLatency.log' for cache_size in cache_sizes]
    result_data = []
    for idx, file in enumerate(result_files):
        with open(file, 'r') as f:
            lines = f.readlines()
            l_p99 = []
            l_p95 = []
            l_p50 = []
            l_aver = []
            l_throughoutput = []
            l_hitrate = []
            # for line in lines:
            line = lines[-1]
            p99, p95, p50, average, throughoutput, hitrate = line.split(' ')
            l_p99.append(float(p99))
            l_p95.append(float(p95))
            l_p50.append(float(p50))
            l_aver.append(float(average))
            l_throughoutput.append(float(throughoutput))
            l_hitrate.append(float(hitrate))
            result_data.append([int(cache_sizes[idx]/32), np.mean(l_p99), np.mean(l_p95), np.mean(l_p50), np.mean(l_aver), np.mean(l_throughoutput), np.mean(l_hitrate)])
    result_data = np.array(result_data)
    df = pd.DataFrame(result_data, columns=['cache size', 'p99', 'p95', 'p50', 'average', 'throughoutput', 'hitrate'])
    target_file = profile_prefix + '.csv'
    df.to_csv(target_file, index=False)


if __name__ == '__main__':
    cache_sizes = [i * 128 for i in range(25)]
    profile_prefix = './data/0724/leveldb_sequential_2400M'
    read_files(cache_sizes, profile_prefix)

    # file_names = [
    #     'bin/mysql_5G_hotspot',
    #     'bin/mysql_5G_uniform',
    #     'bin/tmdb_5G_hotspot',
    #     'bin/tmdb_10G_sequential',
    # ]
    # file_names = [f + '_tailLatency.log' for f in file_names]
    # 
    # for file_name in file_names:
    #     l_p99 = []
    #     l_p95 = []
    #     l_p50 = []
    #     hit_rate = []
    #     with open(file_name, 'r') as f:
    #         lines = f.readlines()
    #         for i, line in enumerate(lines):
    #             if i > 2:
    #                 break
    #             # print(line.split(' '))
    #             p99, p95, p50, hr = line.split(' ')
    #             l_p99.append(float(p99))
    #             l_p95.append(float(p95))
    #             l_p50.append(float(p50))
    #             hit_rate.append(float(hr))
    #         print('{},{},{},{}'.format(np.mean(l_p99), np.mean(l_p95), np.mean(l_p50),np.mean(hit_rate)))
        
