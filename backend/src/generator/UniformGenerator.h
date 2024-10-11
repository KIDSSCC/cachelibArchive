#include<iostream>
#include "generator.h"
using namespace std;

class UniformGenerator: public Generator{
public:
    UniformGenerator(){};
    UniformGenerator(size_t max_records);
    ~UniformGenerator(){};
    void init();
    size_t get_num(std::default_random_engine &generator) override;
private:
    std::uniform_int_distribution<int> uniform_distrib;
};