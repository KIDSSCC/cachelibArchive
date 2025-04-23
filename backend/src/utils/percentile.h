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

template <typename T>
void average_and_percentile(const std::vector<T>& latencies, T* average_latency, T* p99_latency)
{
    std::vector<T> top_elements;
    const size_t max_size = latencies.size() * 0.001 + 1;
    T average_latency_ = 0;

    for (const auto& value : latencies)
    {
        average_latency_ += value;
        if (top_elements.size() < max_size)
        {
            top_elements.push_back(value);
            if (top_elements.size() == max_size)
            {
                std::sort(top_elements.begin(), top_elements.end(), std::greater<int>());
            }
        }
        else 
        {
            if(value > top_elements.back())
            {
                top_elements[max_size-1] = value;
                for(size_t i=top_elements.size()-1;i>0;i--)
                {
                    if(top_elements[i] > top_elements[i-1])
                    {
                        swap(top_elements[i], top_elements[i-1]);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            
        }
    }
    *average_latency = average_latency_/latencies.size();
    *p99_latency = top_elements.back();
    return;
}