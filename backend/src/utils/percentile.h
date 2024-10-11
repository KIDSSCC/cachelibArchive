#pragma once

#include "common.h"

template <typename T>
inline T percentile(const std::vector<T>& latencies, double percentile)
{
    if (latencies.empty())
    {
        return 0;
    }

    std::vector<T> sorted(latencies);
    std::sort(sorted.begin(), sorted.end());

    double index = percentile * (sorted.size() - 1);
    size_t lower = std::floor(index);
    size_t upper = std::ceil(index);

    if (lower == upper)
    {
        return sorted[lower];
    }

    return sorted[lower] + (sorted[upper] - sorted[lower]) * (index - lower);
}

template <typename T>
inline T average(const std::vector<T>& latencies)
{
    if (latencies.empty())
    {
        return 0;
    }
    T average_latency = 0;
    for(const auto& value: latencies)
    {
        average_latency +=value;
    }
    return average_latency/latencies.size();
}