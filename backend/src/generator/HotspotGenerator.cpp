#include "HotspotGenerator.h"

HotspotGenerator::HotspotGenerator(size_t max_records, double proportion, double alpha): Generator(max_records){
    init(proportion, alpha);
}

void HotspotGenerator::init(double proportion, double alpha){
    std::vector<double> intervals = {0, proportion * max_records, double(max_records)};
    std::vector<double> weights = {alpha, 1 - alpha};
    hotspot_distrib = std::piecewise_constant_distribution<double>(intervals.begin(), intervals.end(), weights.begin());}

size_t HotspotGenerator::get_num(std::default_random_engine &generator){
    return int(hotspot_distrib(generator)) % max_records;
}