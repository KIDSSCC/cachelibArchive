#include "generator.h"

using namespace std;

Generator::Generator(Distribution distibution, int workingset_size, vector<double> params){
    d_type = distibution;
    max_records = workingset_size;

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
    size_t key = 0;
    switch (d_type)
    {
        case D_ZIPFIAN:
            key = zipfian_distrib(generator) % max_records;
            break;
        case D_UNIFORM:
            key = uniform_distrib(generator) % max_records;
            break;
        case D_HOTSPOT:
            key = int(hotspot_distrib(generator)) % max_records;
            break;
        case D_EXPONENTIAL:
            key =  int(exponential_distrib(generator)) % max_records;
            break;
        case D_SEQUENTIAL:
            key = sequential_counter++;
            if(sequential_counter >= (int)max_records)
                sequential_counter = 0; 
        default:
            break;
    }
    return key;
}

void Generator::print(){
    string distribution_name = "";
    switch (d_type)
    {
        case D_ZIPFIAN:
            distribution_name = "Zipfian";
            break;
        case D_UNIFORM:
            distribution_name = "Uniform";
            break;
        case D_HOTSPOT:
            distribution_name = "Hotspot";
            break;
        case D_EXPONENTIAL:
            distribution_name = "Exponential";
            break;
        case D_SEQUENTIAL:
            distribution_name = "Sequential";
        default:
            break;
    }
    cout << "----- Generator Info -----\n";
    cout << "Distribution: " << distribution_name << endl;
    cout << "Max records: " << max_records << endl;
    cout << "----- Generator End -----\n";
}
