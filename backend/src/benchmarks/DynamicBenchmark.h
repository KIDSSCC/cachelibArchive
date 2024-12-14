#pragma once

#include "benchmark.h"
#include "backend/backend.h"
#include "utils/zipfian.h"
#include "utils/randstring.h"
#include "cache/AutoIncCache.h"
#include "generator/generator.h"
#include CONFIG_FILE 
#include <random>
#include <ctime>
#include <numeric>
#include <functional>
#include <memory>

class DynamicBenchmark : public Benchmark
{
public:
    DynamicBenchmark(Backend& backend): Benchmark(backend){};
    DynamicBenchmark(Backend& backend, std::shared_ptr<Generator>& generator, unsigned int curr_query = MAX_QUERIES);
    ~DynamicBenchmark() = default;
    void init(std::shared_ptr<Generator>& generator, unsigned int curr_query = MAX_QUERIES);
    
    bool create_database();
    bool load_database();
    bool step();
    bool is_end();
    bool cleanup();

    bool read_record(int key, std::vector<std::string>& results);
    bool insert_record(int key, std::vector<std::string>& values);
    
    // std::string generate_read_sql(int key);
    // std::string generate_insert_sql(int key, std::vector<std::string>& values);

public:
    unsigned int max_records = MAX_RECORDS; // number of records in the database
    unsigned int max_fields = MAX_FIELDS; // number of fields in each record
    unsigned int max_field_size = MAX_FIELD_SIZE; // size of each field (in chars)
    unsigned int max_query = MAX_QUERIES; // number of queries to execute
    double query_proportion = QUERY_PROPORTION; // proportion of read queries, the rest are insert queries

    // unsigned int next_key = 0; // this has moved to Backend class to avoid duplicate key error across threads.
    std::random_device rd;
    std::mt19937 rng;
    std::default_random_engine random_engine; // generator used for zipfian distribution
    std::shared_ptr<Generator> generate_;
    unsigned int current_query = 0;
};
