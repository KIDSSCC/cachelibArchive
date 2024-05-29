#include "common.h"
#include "benchmarks/YCSBBenchmark.h"
#include "backend/MysqlBackend.h"
#include "backend/MongoDBBackend.h"
#include "backend/LevelDBBackend.h"
#include "backend/SQLiteBackend.h"
#include "utils/percentile.h"
#include "cache/clientAPI.h"
#include "config.h"
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

std::atomic<int> g_next_insert_key = 0;

int main(int argc, char* argv[]) {
    bool cache_enabled = false;
    bool do_prepare = true;
    bool do_run = true;
    int num_threads = 1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cache") {
            cache_enabled = true;
        } else if (arg == "--threads") {
            if (i + 1 < argc) {
                num_threads = std::stoi(argv[i + 1]);
            }
        } else if (arg == "--prepare") {
            do_run = false;
        } else if (arg == "--run") {
            do_prepare = false;
        } else if (arg == "--help") {
            std::cout << "Usage: sqlcache [options]\n"
                      << "Options:\n"
                      << "  --cache: enable cache\n"
                      << "  --threads <num>: number of threads\n"
                      << "  --prepare: only prepare the database\n"
                      << "  --run: only run the benchmark\n"
                      << "if no options are provided, both preparation and benchmark will be run\n";
            return 0;
        }
    }
    
    CachelibClient unified_cache {};
    unified_cache.addpool(UNIFIED_CACHE_POOL);

    // aggregate the results of multiple threads
    double total_throughput = 0;
    unsigned int total_hit_count = 0;
    unsigned int total_records_executed = 0;
    std::vector<unsigned int> total_latencies;
    std::mutex total_latencies_mutex;

    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
        YCSBBenchmark benchmark(backend);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
    }

    if (do_run) {
        g_next_insert_key = MAX_RECORDS;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([cache_enabled, i, 
                                &total_throughput, &total_hit_count, &total_records_executed, 
                                &total_latencies, &total_latencies_mutex, &unified_cache]() {

                BACKEND backend(0);
                if (cache_enabled) {
                    backend.enable_cache(unified_cache);
                }
                
                YCSBBenchmark benchmark(backend);
                benchmark.run();
                double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;
                std::cout << "Thread " << i << " Throughput: " << throughput << " records/s" << std::endl;
                std::vector<unsigned int> latencies = benchmark.latencies_ns;
                unsigned int percentile_99 = percentile(latencies, 0.99);
                unsigned int percentile_95 = percentile(latencies, 0.95);
                unsigned int percentile_50 = percentile(latencies, 0.50);
                std::cout << "Thread " << i << " 99th percentile: " << percentile_99 << " ns" << std::endl;
                std::cout << "Thread " << i << " 95th percentile: " << percentile_95 << " ns" << std::endl;
                std::cout << "Thread " << i << " 50th percentile: " << percentile_50 << " ns" << std::endl;
                unsigned int hit_count = backend.hit_count;
                unsigned int total_count = benchmark.records_executed;
                double hitrate = (double) hit_count / (double) total_count;
                std::cout << "Thread " << i << " Hitrate: " << hitrate << std::endl;

                // aggregate the results
                total_throughput += throughput;
                total_hit_count += hit_count;
                total_records_executed += total_count;
                {
                    std::lock_guard<std::mutex> lock(total_latencies_mutex);
                    total_latencies.insert(total_latencies.end(), latencies.begin(), latencies.end());
                }
            });
        }
    

        for (auto& thread : threads) {
            thread.join();
        }

        BACKEND backend(0);
        backend.clean_up();

        std::cout << "Total Throughput: " << total_throughput << " records/s" << std::endl;
        unsigned int total_percentile_99 = percentile(total_latencies, 0.99);
        unsigned int total_percentile_95 = percentile(total_latencies, 0.95);
        unsigned int total_percentile_50 = percentile(total_latencies, 0.50);
        std::cout << "Total 99th percentile: " << total_percentile_99 << " ns" << std::endl;
        std::cout << "Total 95th percentile: " << total_percentile_95 << " ns" << std::endl;
        std::cout << "Total 50th percentile: " << total_percentile_50 << " ns" << std::endl;
        double total_hitrate = (double) total_hit_count / (double) total_records_executed;
        std::cout << "Total Hitrate: " << total_hitrate << std::endl;
    }

    return 0;
}
