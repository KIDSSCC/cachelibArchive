#pragma once

#include "common.h"
#include "config.h"
#include "backend/backend.h"

class Benchmark
{
public:
    Backend& backend;
    std::string name;
    Benchmark(Backend& backend) : backend(backend){};
    virtual ~Benchmark() {};
    virtual bool create_database() = 0;
    virtual bool load_database() = 0;
    virtual bool step() = 0;
    virtual bool is_end();
    void prepare();
    void run();
    virtual bool cleanup() = 0;

    unsigned int records_executed = 0;
    unsigned int millis_elapsed = 0; // in milliseconds

    std::vector<unsigned int> latencies_ns;
};