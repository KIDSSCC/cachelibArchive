#pragma once

#include<iostream>
#include<random>
#include <memory>
#include "utils/zipfian.h"
using namespace std;

enum Distribution{
    D_ZIPFIAN,
    D_HOTSPOT,
    D_UNIFORM,
    D_EXPONENTIAL,
    D_SEQUENTIAL
};
class Generator{
public:
    Generator(){}
    Generator(Distribution distibution, int workingset_size, vector<double> params);
    ~Generator(){}
    size_t get_num(std::default_random_engine &generator);

    void print();

private:
    Distribution d_type;
    size_t max_records = 0;

    zipfian_int_distribution<int> zipfian_distrib;

    uniform_int_distribution<int> uniform_distrib;

    piecewise_constant_distribution<double> hotspot_distrib;

    exponential_distribution<double> exponential_distrib;

    int sequential_counter = 0;




};