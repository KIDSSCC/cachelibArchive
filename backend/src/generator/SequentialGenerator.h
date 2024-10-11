#include<iostream>
#include "generator.h"
#include "utils/zipfian.h"
using namespace std;

class SequentialGenerator: public Generator{
public:
    SequentialGenerator(){};
    SequentialGenerator(size_t max_records, int sequential_startidx);
    ~SequentialGenerator(){};
    void init(int sequential_startidx);
    size_t get_num(std::default_random_engine &generator) override;
private:
    int sequential_counter = 0;
};