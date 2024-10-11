#include "ExponentialGenerator.h"

ExponentialGenerator::ExponentialGenerator(size_t max_records, double lambda): Generator(max_records){
    init(lambda);
}

void ExponentialGenerator::init(double lambda){
    exponential_distrib = std::exponential_distribution<double>(lambda);
}

size_t ExponentialGenerator::get_num(std::default_random_engine &generator){
    return int(exponential_distrib(generator)) % max_records;
}