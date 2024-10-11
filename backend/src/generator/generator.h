#include<iostream>
#include<random>
using namespace std;

class Generator{
public:
    Generator(){}
    Generator(size_t max_records_){
        max_records = max_records_;
    }
    ~Generator(){}
    virtual size_t get_num(std::default_random_engine &generator) = 0;

    size_t max_records = 0;
};