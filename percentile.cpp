#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <memory>
#include <queue>
#include <random>

using namespace std;

template <typename T>
inline void percentile(const std::vector<T>& latencies, double percentile, T* res)
{
    std::vector<T> sorted(latencies);
    std::sort(sorted.begin(), sorted.end());

    double index = percentile * (sorted.size() - 1);
    size_t lower = std::floor(index);
    size_t upper = std::ceil(index);
    for(int i=index;i<sorted.size();i++){
        cout<<sorted[i]<<", ";
    }
    cout<<endl;

    *res = sorted[lower] + (sorted[upper] - sorted[lower]) * (index - lower);
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
            top_elements.back() = value;
            for(int i=top_elements.size()-1;i>0;i--)
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
    for(int i=0;i<top_elements.size();i++){
        cout<<top_elements[i]<<",";
    }
    cout<<endl;
    *average_latency = average_latency_/latencies.size();
    *p99_latency = top_elements.back();

    return;
}

int main()
{
    cout<< "hello, world" << endl;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0, 1000.0);
    double random_number = dis(gen);

    vector<double> origindata;
    for(int i=0;i<10000;i++){
        origindata.push_back(dis(gen));
    }
    cout<< "prepare data" << endl;
    double res1 = 0.0;
    double res2 = 0.0;
    double aver = 0.0;

    average_and_percentile(origindata, &aver, &res2);
    percentile(origindata, 0.999, &res1);

    cout<< "res1 is: " << res1 << " res2 is: " << res2 << endl;
    cout<< "aver is: " << aver<< endl;
}
