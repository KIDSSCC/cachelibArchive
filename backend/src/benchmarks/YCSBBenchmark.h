#pragma once

#include "benchmark.h"
#include "backend/backend.h"
#include "utils/zipfian.h"
#include "utils/randstring.h"
#include "cache/AutoIncCache.h"
#include "config.h"
#include <random>
#include <ctime>
#include <numeric>

class YCSBBenchmark : public Benchmark
{
public:
    YCSBBenchmark(Backend& backend);
    ~YCSBBenchmark() = default;
    bool create_database();
    bool load_database();
    bool step();
    bool is_end();
    bool cleanup();

    bool read_record(int key, std::vector<std::string>& results);
    bool insert_record(int key, std::vector<std::string>& values);
    
    std::string generate_read_sql(int key);
    std::string generate_insert_sql(int key, std::vector<std::string>& values);

public:
    std::default_random_engine generator; // generator used for zipfian distribution
    #if DISTRIBUTION == DISTRIBUTION_ZIPFIAN
        zipfian_int_distribution<int> zipfian_distrib;
    #elif DISTRIBUTION == DISTRIBUTION_UNIFORM
        std::uniform_int_distribution<int> uniform_distrib;
    #elif DISTRIBUTION == DISTRIBUTION_SEQUENTIAL
        unsigned int sequential_counter = 0;
    #elif DISTRIBUTION == DISTRIBUTION_HOTSPOT
        std::piecewise_constant_distribution<double> hotspot_distrib;
    #elif DISTRIBUTION == DISTRIBUTION_EXPONENTIAL
        std::exponential_distribution<double> exponential_distrib;
    #endif

    // unsigned int next_key = 0; // this has moved to Backend class to avoid duplicate key error across threads.
    unsigned int current_query = 0;
    std::random_device rd;
    std::mt19937 rng;

    unsigned int max_records = MAX_RECORDS; // number of records in the database
    unsigned int max_fields = MAX_FIELDS; // number of fields in each record
    unsigned int max_field_size = MAX_FIELD_SIZE; // size of each field (in chars)
    unsigned int max_query = MAX_QUERIES; // number of queries to execute
    double query_proportion = QUERY_PROPORTION; // proportion of read queries, the rest are insert queries
};
