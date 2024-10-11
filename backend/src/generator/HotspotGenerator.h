#include<iostream>
#include "generator.h"
using namespace std;

class HotspotGenerator: public Generator{
public:
    HotspotGenerator(){};
    HotspotGenerator(size_t max_records, double proportion, double alpha);
    ~HotspotGenerator(){};
    void init(double proportion, double alpha);
    size_t get_num(std::default_random_engine &generator) override;
private:
    std::piecewise_constant_distribution<double> hotspot_distrib;
};