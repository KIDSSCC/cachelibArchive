#include "ZipfianGenerator.h"

ZipfianGenerator::ZipfianGenerator(size_t max_records, double zipfian_skew): Generator(max_records){
    init(zipfian_skew);
}

void ZipfianGenerator::init(double zipfian_skew){
    zipfian_distrib = zipfian_int_distribution<int>(0, max_records - 1, zipfian_skew);
}

size_t ZipfianGenerator::get_num(std::default_random_engine &generator){
    return zipfian_distrib(generator) % max_records;
}