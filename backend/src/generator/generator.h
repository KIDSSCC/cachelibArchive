#pragma once

#include<iostream>
#include<random>
#include <memory>
#include "utils/zipfian.h"
using namespace std;

// 支持的query分布类型
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
    size_t get_num(std::default_random_engine &engine);
    size_t get_max(){return this->max_records;}

    void print();

private:
    // 当前query分布类型
    Distribution d_type;

    // 生成query的最大编号
    size_t max_records = 0;

    // 各类分布的生成器或计数器
    zipfian_int_distribution<int> zipfian_distrib;

    uniform_int_distribution<int> uniform_distrib;

    piecewise_constant_distribution<double> hotspot_distrib;

    exponential_distribution<double> exponential_distrib;

    int sequential_counter = 0;




};