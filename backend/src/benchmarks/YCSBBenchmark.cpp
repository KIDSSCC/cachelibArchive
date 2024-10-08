#include "YCSBBenchmark.h"
#include <thread>

YCSBBenchmark::YCSBBenchmark(Backend& backend, unsigned int sequential_startidx, int threadId, bool WhetherSequence, unsigned int CURR_QUERY)
    : Benchmark(backend) { 
        //kidsscc: reinitialize max_query
        max_query = CURR_QUERY;
        latencies_ns.reserve(max_query);
        whether_sequence = WhetherSequence;
		this->threadId = threadId;

        name = "YCSB";

        std::thread::id thread_id = std::this_thread::get_id();
        std::hash<std::thread::id> hash_func;
        size_t hashed_id = hash_func(thread_id);
        size_t seed = static_cast<size_t>(std::time(0))
                      ^ (hashed_id + 0x9e3779b9
                      + (static_cast<size_t>(std::time(0)) << 6)
                      + (static_cast<size_t>(std::time(0)) >> 2));
        generator = std::default_random_engine(seed);

        if(!whether_sequence)
        {
            #if DISTRIBUTION == DISTRIBUTION_ZIPFIAN
                zipfian_distrib = zipfian_int_distribution<int>(0, max_records - 1, ZIPFIAN_SKEW);
            #elif DISTRIBUTION == DISTRIBUTION_UNIFORM
                uniform_distrib = std::uniform_int_distribution<int>(0, max_records - 1);
            #elif DISTRIBUTION == DISTRIBUTION_SEQUENTIAL
                sequential_counter = sequential_startidx;
            #elif DISTRIBUTION == DISTRIBUTION_HOTSPOT
                std::vector<double> intervals = {0, HOTSPOT_PROPORTION * max_records, double(max_records)};
                std::vector<double> weights = {HOTSPOT_ALPHA, 1 - HOTSPOT_ALPHA};
                hotspot_distrib = std::piecewise_constant_distribution<double>(intervals.begin(), intervals.end(), weights.begin());
            #elif DISTRIBUTION == DISTRIBUTION_EXPONENTIAL
                exponential_distrib = std::exponential_distribution<double>(EXPONENTIAL_LAMBDA);
            #endif 
        }else
        {
            unified_sequential_counter = 0;
        }
        
        rng = std::mt19937(rd());
    }

bool YCSBBenchmark::create_database() {
    debug(
        std::cout << "YCSBBenchmark: creating database" << std::endl;
    );
    return backend.create_database();
}

bool YCSBBenchmark::load_database() {
    debug(
        std::cout << "YCSBBenchmark: loading database" << std::endl;
    );
    // insert records into the database
    for (unsigned int i = 0; i < max_records; i++) {
        std::vector<std::string> values;
        for (unsigned int j = 0; j < max_fields; j++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!backend.insert_record(i, values)) {
            std::cout << "YCSBBenchmark: load_database: insert record failed" << std::endl;
            return false;
        }
    }
    g_next_insert_key = max_records;
    return true;
}

bool YCSBBenchmark::step() {
    // excute different queries based on the proportion
    if (rng() % 100 < query_proportion * 100) {
        // read query
        std::vector<std::string> results;
        int key = unified_sequential_counter++;
        if (unified_sequential_counter >= max_records) {
                unified_sequential_counter = 0;
            }
        if(!whether_sequence)
        {
            #if DISTRIBUTION == DISTRIBUTION_ZIPFIAN
                key = zipfian_distrib(generator) % max_records;
            #elif DISTRIBUTION == DISTRIBUTION_UNIFORM
                key = uniform_distrib(generator) % max_records;
            #elif DISTRIBUTION == DISTRIBUTION_SEQUENTIAL
                key = sequential_counter++;
                if (sequential_counter >= max_records) {
                    sequential_counter = 0;
                }
            #elif DISTRIBUTION == DISTRIBUTION_HOTSPOT
                key = int(hotspot_distrib(generator)) % max_records;
            #elif DISTRIBUTION == DISTRIBUTION_EXPONENTIAL
                key = int(exponential_distrib(generator)) % max_records; // its not precise but it works.
            #endif
        }
        // debug(
        //     std::cout << "YCSBBenchmark: read query: " << key << std::endl;
        // );
        if (!read_record(key, results)) {
            std::cout << "YCSBBenchmark: read query failed" << std::endl;
            return false;
        }
        // debug
        // debug(
        //     std::cout << "YCSBBenchmark: read query result: " << std::endl;
        //     for (auto& result : results) {
        //         std::cout << result << " ";
        //     }
        //     std::cout << std::endl;
        // );
    } else {
        //insert operation
        std::vector<std::string> values;
        for (unsigned int i = 0; i < max_fields; i++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!insert_record(g_next_insert_key++, values)) {
            std::cout << "YCSBBenchmark: insert query failed" << std::endl;
            return false;
        }
    }
    current_query++;
    return true;
}

bool YCSBBenchmark::read_record(int key, std::vector<std::string>& results) {
    if (!backend.read_record(key, results)) {
        std::cout << "YCSBBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    // make sure results are not optimized away
    if (results.size() == 0) {
        std::cout << "YCSBBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    return true;
}

bool YCSBBenchmark::insert_record(int key, std::vector<std::string>& values) {
    if (!backend.insert_record(key, values)) {
        std::cout << "YCSBBenchmark: insert_record: " << key << " failed" << std::endl;
        return false;
    }
    max_records++;
    return true;
}

bool YCSBBenchmark::is_end() {
    if(max_query==0)
        return false;
    return current_query >= max_query;
}

bool YCSBBenchmark::cleanup() {
    return true;
}
