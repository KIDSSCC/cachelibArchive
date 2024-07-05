#include "common.h"
#include "benchmarks/YCSBBenchmark.h"
#include "backend/MysqlBackend.h"
#include "backend/MongoDBBackend.h"
#include "backend/LevelDBBackend.h"
#include "backend/SQLiteBackend.h"
#include "backend/TmdbBackend.h"
#include "utils/percentile.h"
//#include "cache/clientAPI.h"
#include "clientAPI.h"
#include "config.h"
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <fstream>

#include <iostream>
#include <chrono>
#include <thread>


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
    
    

    // aggregate the results of multiple threads
    double total_throughput = 0;
    unsigned int total_hit_count = 0;
    unsigned int total_records_executed = 0;
    std::vector<unsigned int> total_latencies;
    std::mutex total_latencies_mutex;


    CachelibClient unified_cache {};
    unified_cache.addpool(UNIFIED_CACHE_POOL);
    //kidsscc:wait 10 sec adjust pool size
    this_thread::sleep_for(std::chrono::seconds(10));


    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
        //kidsscc:write to cache in prepare phase
        // if(cache_enabled){
        //     backend.enable_cache(unified_cache);
        // }
        YCSBBenchmark benchmark(backend);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
        // std::string prepare_finish = string(UNIFIED_CACHE_POOL) + string("prepare.log");
        // std::ofstream file(prepare_finish, std::ios::app);
        // file.close();
    }
    if (do_run) {
    // int count = 2000;
    // while(count > 0){
    //     count --;
        for(int itea=0;itea<2;itea++){
            g_next_insert_key = MAX_RECORDS;
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; i++) {
                threads.emplace_back([itea, cache_enabled, i, 
                                    &total_throughput, &total_hit_count, &total_records_executed, 
                                    &total_latencies, &total_latencies_mutex]() {

                    BACKEND backend(0);
                    CachelibClient cache_client {};
                    cache_client.addpool(UNIFIED_CACHE_POOL);
                    if (cache_enabled) {
                        backend.enable_cache(cache_client);
                    }

                    //kidsscc: calculate max_querys
                    unsigned int curr_querys =(itea == 0?10485760:MAX_QUERIES);

                    YCSBBenchmark benchmark(backend);
                    // YCSBBenchmark benchmark(backend, curr_querys);
                    benchmark.run();
                    double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;
                    OUTPUT << (double) benchmark.records_executed << std::endl;
                    unsigned int hit_count = backend.hit_count;
                    unsigned int total_count = benchmark.records_executed;
                    std::vector<unsigned int> latencies = benchmark.latencies_ns;
                    // unsigned int percentile_99 = percentile(latencies, 0.99);
                    // unsigned int percentile_95 = percentile(latencies, 0.95);
                    // unsigned int percentile_50 = percentile(latencies, 0.50);
                    // double hitrate = (double) hit_count / (double) total_count;
                    // std::cout << "Thread " << i << " Throughput: " << throughput << " records/s" << std::endl;
                    // std::cout << "Thread " << i << " 99th percentile: " << percentile_99 << " ns" << std::endl;
                    // std::cout << "Thread " << i << " 95th percentile: " << percentile_95 << " ns" << std::endl;
                    // std::cout << "Thread " << i << " 50th percentile: " << percentile_50 << " ns" << std::endl;
                    // std::cout << "Thread " << i << " Hitrate: " << hitrate << std::endl;

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
            std::string workload_log = string(UNIFIED_CACHE_POOL) + string("_latency.log");
            std::ofstream log_file(workload_log, std::ios::app);
            if(itea == 1){
                unsigned int average_latency = average(total_latencies);
                unsigned int total_percentile_99 = percentile(total_latencies, 0.99);
                unsigned int total_percentile_95 = percentile(total_latencies, 0.95);
                unsigned int total_percentile_50 = percentile(total_latencies, 0.50);
                double total_hitrate = (double) total_hit_count / (double) total_records_executed;
                OUTPUT << "Total Throughput: " << total_throughput << " records/s" << std::endl;
                OUTPUT << "Total 99th percentile: " << total_percentile_99 << " ns" << std::endl;
                OUTPUT << "Total 95th percentile: " << total_percentile_95 << " ns" << std::endl;
                OUTPUT << "Total 50th percentile: " << total_percentile_50 << " ns" << std::endl;
                OUTPUT << "Total average percentile: " << average_latency << " ns" << std::endl;                
                OUTPUT << "Total Hitrate: " << total_hitrate << std::endl;
            }
            log_file.close();
            
            total_throughput = 0;
            total_hit_count = 0;
            total_records_executed = 0;
            total_latencies.clear();
            total_latencies.shrink_to_fit();
        }
    }

    return 0;
}
