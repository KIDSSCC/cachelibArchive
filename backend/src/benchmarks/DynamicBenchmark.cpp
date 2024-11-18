#include "DynamicBenchmark.h"
#include <thread>
#include <memory>

DynamicBenchmark::DynamicBenchmark(Backend& backend, std::shared_ptr<Generator>& generator, unsigned int curr_query)
    : Benchmark(backend) { 
        init(generator, curr_query);
}

void DynamicBenchmark::init(std::shared_ptr<Generator>& generator, unsigned int curr_query) {
    max_query = curr_query;
    latencies_ns.reserve(max_query);

    name = "Dynamic";

    std::thread::id thread_id = std::this_thread::get_id();
    std::hash<std::thread::id> hash_func;
    size_t hashed_id = hash_func(thread_id);
    size_t seed = static_cast<size_t>(std::time(0))
                    ^ (hashed_id + 0x9e3779b9
                    + (static_cast<size_t>(std::time(0)) << 6)
                    + (static_cast<size_t>(std::time(0)) >> 2));
    random_engine = std::default_random_engine(seed);
    generate_ = generator;
    rng = std::mt19937(rd());
    
}

bool DynamicBenchmark::create_database() {
    debug(
        std::cout << "DynamicBenchmark: creating database" << std::endl;
    );
    return backend.create_database();
}

bool DynamicBenchmark::load_database() {
    debug(
        std::cout << "DynamicBenchmark: loading database" << std::endl;
    );
    // insert records into the database
    for (unsigned int i = 0; i < max_records; i++) {
        std::vector<std::string> values;
        for (unsigned int j = 0; j < max_fields; j++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!backend.insert_record(i, values)) {
            std::cout << "DynamicBenchmark: load_database: insert record failed" << std::endl;
            return false;
        }
    }
    g_next_insert_key = max_records;
    return true;
}

bool DynamicBenchmark::step() {
    // excute different queries based on the proportion
    if (rng() % 100 < query_proportion * 100) {
        // read query
        std::vector<std::string> results;
        int key = generate_->get_num(random_engine);
        if (!read_record(key, results)) {
            std::cout << "DynamicBenchmark: read query failed" << std::endl;
            return false;
        }
    } else {
        //insert operation
        std::vector<std::string> values;
        for (unsigned int i = 0; i < max_fields; i++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!insert_record(g_next_insert_key++, values)) {
            std::cout << "DynamicBenchmark: insert query failed" << std::endl;
            return false;
        }
    }
    current_query++;
    return true;
}

bool DynamicBenchmark::read_record(int key, std::vector<std::string>& results) {
    if (!backend.read_record(key, results)) {
        std::cout << "DynamicBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    // make sure results are not optimized away
    if (results.size() == 0) {
        std::cout << "DynamicBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    return true;
}

bool DynamicBenchmark::insert_record(int key, std::vector<std::string>& values) {
    if (!backend.insert_record(key, values)) {
        std::cout << "DynamicBenchmark: insert_record: " << key << " failed" << std::endl;
        return false;
    }
    max_records++;
    return true;
}

bool DynamicBenchmark::is_end() {
    if(max_query==0)
        return false;
    return current_query >= max_query;
}

bool DynamicBenchmark::cleanup() {
    return true;
}
