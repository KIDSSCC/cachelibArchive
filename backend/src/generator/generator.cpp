#include "generator.h"

using namespace std;

Generator::Generator(Distribution distibution, vector<double>& params, int workingset_size, int queries){
    d_type = distibution;
    max_records = workingset_size;
    max_queries = queries;

    //for hootspot
    std::vector<double> intervals = {0};
    std::vector<double> weights;
    
    switch (d_type)
    {
        case D_ZIPFIAN:
            zipfian_distrib = zipfian_int_distribution<int>(0, max_records - 1, params[0]);
            break;
        case D_UNIFORM:
            uniform_distrib = std::uniform_int_distribution<int>(0, max_records - 1);
            break;
        case D_HOTSPOT:
            intervals.push_back(params[0] * max_records);
            intervals.push_back(double(max_records));
            weights.push_back(params[1]);
            weights.push_back(1 - params[1]);

            hotspot_distrib = std::piecewise_constant_distribution<double>(intervals.begin(), intervals.end(), weights.begin());
            break;
        case D_EXPONENTIAL:
            exponential_distrib = std::exponential_distribution<double>(params[0]);
            break;
        case D_SEQUENTIAL:
            sequential_counter = 0;
        default:
            break;
    }
}

size_t Generator::get_num(std::default_random_engine &generator){
    switch (d_type)
    {
        case D_ZIPFIAN:
            return zipfian_distrib(generator) % max_records;
        case D_UNIFORM:
            return uniform_distrib(generator) % max_records;
        case D_HOTSPOT:
            return int(hotspot_distrib(generator)) % max_records;
        case D_EXPONENTIAL:
            return int(exponential_distrib(generator)) % max_records;
        case D_SEQUENTIAL:
            size_t key = sequential_counter++;
            if(sequential_counter >= max_records)
                sequential_counter = 0; 
            return key;
        default:
            return 0;
    }
}


