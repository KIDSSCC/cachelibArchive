#include "UniformGenerator.h"

UniformGenerator::UniformGenerator(size_t max_records): Generator(max_records){
    init();
}

void UniformGenerator::init(){
    uniform_distrib = std::uniform_int_distribution<int>(0, max_records - 1);
}

size_t UniformGenerator::get_num(std::default_random_engine &generator){
    return uniform_distrib(generator) % max_records;
}