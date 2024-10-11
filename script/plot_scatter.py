import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import struct

def read_binary_file(filepath):
    with open(filepath, "rb") as f:
        # Read the size of the vector
        size = struct.unpack('I', f.read(4))[0]
        # Read the vector data
        data = struct.unpack(f'{size}I', f.read(size * 4))
    return data

def remove_outliers_percentile(data, lower_percentile=0.001, upper_percentile=0.999):
    # Calculate lower and upper percentiles
    lower_bound = np.percentile(data, lower_percentile * 100)
    upper_bound = np.percentile(data, upper_percentile * 100)
    
    # Filter out outliers
    filtered_data = [x for x in data if lower_bound <= x <= upper_bound]
    
    return filtered_data

file_path = "mongodb_100M_hotspot_cache10M"
data = read_binary_file(file_path)
filtered_data = remove_outliers_percentile(data)

# scatter plot
plt.figure(figsize=(12, 8))
plt.scatter(range(len(filtered_data)), filtered_data, s=1)
plt.title("Latency Scatter Plot")
plt.xlabel("Request Index")
plt.ylabel("Latency (ns)")
plt.savefig("latency_scatter_plot.png")