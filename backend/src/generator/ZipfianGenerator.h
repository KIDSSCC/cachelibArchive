#include<iostream>
#include "generator.h"
#include "utils/zipfian.h"
using namespace std;

class ZipfianGenerator: public Generator{
public:
    ZipfianGenerator(){};
    ZipfianGenerator(size_t max_records, double zipfian_skew);
    ~ZipfianGenerator(){};
    void init(double zipfian_skew);
    size_t get_num(std::default_random_engine &generator) override;
private:
    zipfian_int_distribution<int> zipfian_distrib;
};