#include "SequentialGenerator.h"

SequentialGenerator::SequentialGenerator(size_t max_records, int sequential_startidx): Generator(max_records){
    init(sequential_startidx);
}

void SequentialGenerator::init(int sequential_startidx){
    sequential_counter = sequential_startidx;
}

size_t SequentialGenerator::get_num(std::default_random_engine &generator){
    int key = sequential_counter++;
    if (sequential_counter >= max_records) {
        sequential_counter = 0;
    }
    return key;
}