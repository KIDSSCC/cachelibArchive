#include<iostream>
#include "generator.h"
#include "utils/zipfian.h"
using namespace std;

class ExponentialGenerator: public Generator{
public:
    ExponentialGenerator(){};
    ExponentialGenerator(size_t max_records, double lambda);
    ~ExponentialGenerator(){};
    void init(double lambda);
    size_t get_num(std::default_random_engine &generator) override;
private:
    std::exponential_distribution<double> exponential_distrib;
};