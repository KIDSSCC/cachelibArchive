#include "common.h"
#include "benchmarks/YCSBBenchmark.h"
#include "backend/MysqlBackend.h"
#include "backend/MongoDBBackend.h"
#include "backend/LevelDBBackend.h"
#include "backend/SQLiteBackend.h"
#include "backend/TmdbBackend.h"
#include "utils/percentile.h"
#include "utils/save_vector.h"
#include "clientAPI.h"
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
    int run_times = 0;
    int warmup_times = 0;
    std::string profile_file = "";
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
        } else if (arg == "--warmup") {
            do_prepare = false;
            if (i + 1 < argc) {
                warmup_times = std::stoi(argv[i + 1]);
            }
        } else if (arg == "--run") {
            do_prepare = false;
            if (i + 1 < argc) {
                run_times = std::stoi(argv[i + 1]);
            }
        } else if (arg == "--profile") {
            if (i + 1 < argc) {
                profile_file = argv[i + 1];
            }

        } else if (arg == "--help") {
            std::cout << "Usage: sqlcache [options]\n"
                      << "Options:\n"
                      << "  --cache: enable cache\n"
                      << "  --threads <num>: number of threads\n"
                      << "  --prepare: only prepare the database\n"
                      << "  --warmup <num>: run the benchmark for <num> times as warmup\n"
                      << "  --run <num>: run the benchmark for <num> times\n"
                      << "  --profle <file>: dump the latencies to a file\n"
                      << "  --help: show this message\n"
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

    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
        //kidsscc:write to cache in prepare phase
        if(cache_enabled){
            CachelibClient unified_cache;
            unified_cache.addpool(UNIFIED_CACHE_POOL);
            backend.enable_cache(unified_cache);
        }
        YCSBBenchmark benchmark(backend);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
    }


    int total_times = warmup_times + run_times;
    // when prepare, warmup and run need to be ignored
    // do_run = total_times>0?true:do_run;
    if(!do_run)
        return 0;
    while (total_times--) {
        g_next_insert_key = MAX_RECORDS;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([cache_enabled, i, 
                                &total_throughput, &total_hit_count, &total_records_executed, 
                                &total_latencies, &total_latencies_mutex]() {
                //for multithreads, may need to create cache client for every thread, utilizing multiple SHM to realize property
                CachelibClient cacheclient;
                cacheclient.addpool(UNIFIED_CACHE_POOL);
                BACKEND backend(0);
                if (cache_enabled) {
                    backend.enable_cache(cacheclient);
                }
                
                YCSBBenchmark benchmark(backend);
                benchmark.run();
                double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;
                
                std::vector<unsigned int> latencies = benchmark.latencies_ns;
                unsigned int percentile_99 = percentile(latencies, 0.99);
                unsigned int percentile_95 = percentile(latencies, 0.95);
                unsigned int percentile_50 = percentile(latencies, 0.50);

                unsigned int hit_count = backend.hit_count;
                unsigned int total_count = benchmark.records_executed;
                double hitrate = (double) hit_count / (double) total_count;
                OUTPUT << "Thread " << i << " Hitrate: " << hitrate << std::endl;
                OUTPUT << "Thread " << i << " Throughput: " << throughput << " records/s" << std::endl;
                OUTPUT << "Thread " << i << " 99th percentile: " << percentile_99 << " ns" << std::endl;
                OUTPUT << "Thread " << i << " 95th percentile: " << percentile_95 << " ns" << std::endl;
                OUTPUT << "Thread " << i << " 50th percentile: " << percentile_50 << " ns" << std::endl;

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

        // save latencies for further inspection
        if (!profile_file.empty()) {
            save_vector_to_file(total_latencies, profile_file);
        }

        unsigned int avarage_percentile = average(total_latencies);

        unsigned int total_percentile_99 = percentile(total_latencies, 0.99);
        unsigned int total_percentile_95 = percentile(total_latencies, 0.95);
        unsigned int total_percentile_50 = percentile(total_latencies, 0.50);
        double total_hitrate = (double) total_hit_count / (double) total_records_executed;
        OUTPUT << "Total Hitrate: " << total_hitrate << std::endl;
        OUTPUT << "Total Throughput: " << total_throughput << " records/s" << std::endl;
        OUTPUT << "Total 99th percentile: " << total_percentile_99 << " ns" << std::endl;
        OUTPUT << "Total 95th percentile: " << total_percentile_95 << " ns" << std::endl;
        OUTPUT << "Total 50th percentile: " << total_percentile_50 << " ns" << std::endl;
        
        if (total_times < run_times && !profile_file.empty()) {
            // save percentiles and hitrate to a file
            std::ofstream out(profile_file + "_tailLatency.log", std::ios::app);
            out << total_percentile_99 << " " << total_percentile_95 << " " << total_percentile_50 << " " << total_hitrate << std::endl;
        }

        total_throughput = 0;
        total_hit_count = 0;
        total_records_executed = 0;
        total_latencies.clear();
        total_latencies.shrink_to_fit();
    }

    return 0;
}
