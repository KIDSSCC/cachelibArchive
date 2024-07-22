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
#include <cmath>


std::atomic<int> g_next_insert_key = 0;
int main(int argc, char* argv[]) {
    bool cache_enabled = false;
    bool do_prepare = true;
    bool do_warmup = true;
    bool do_run = true;
    int num_threads = 1;
    int run_times = 0;
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
            do_warmup = false;
            do_run = false;
        } else if (arg == "--warmup") {
            do_prepare = false;
            // do_run = false;
            // warm up until the hit rate change is less than 0.2%
            // if (i + 1 < argc) {
            //     warmup_times = std::stoi(argv[i + 1]);
            // }
        } else if (arg == "--run") {
            do_prepare = false;
            do_warmup = false;
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
    double total_usedtime = 0;
    unsigned int total_hit_count = 0;
    unsigned int total_records_executed = 0;
    std::vector<unsigned int> total_latencies;
    std::mutex total_latencies_mutex;

    if (do_prepare) {
        
        BACKEND backend(0); // therad_id = 0 for same table across threads
        //kidsscc:write to cache in prepare phase
        // TODO:fix the bug. declaring the client within the if scope will cause a segment fault
        if(cache_enabled){
            CachelibClient unified_cache;
            unified_cache.addpool(UNIFIED_CACHE_POOL);
            backend.enable_cache(unified_cache);
        }
        
        YCSBBenchmark benchmark(backend);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
    }

    unsigned int sequential_startidx = 0;
    if(do_warmup)
    {
        double lasthitrate = -1;
        double hitrate = -1;
        bool iterate = true;
        int count = 0;
        while(lasthitrate<0||(hitrate-lasthitrate)>0.002){
            
            count++;
            lasthitrate = hitrate;

            g_next_insert_key = MAX_RECORDS;
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; i++){
                threads.emplace_back([cache_enabled, &iterate,
                                &total_hit_count, &total_records_executed, &sequential_startidx]() {
                    CachelibClient cacheclient;
                    cacheclient.addpool(UNIFIED_CACHE_POOL);
                    BACKEND backend(0);
                    if (cache_enabled) {
                        backend.enable_cache(cacheclient);
                    }
                    if(iterate)
                    {
                        YCSBBenchmark benchmark(backend, sequential_startidx, true, MAX_RECORDS);
                        benchmark.run();
                        total_hit_count += backend.hit_count;
                        total_records_executed += benchmark.records_executed;
                        sequential_startidx = (sequential_startidx + MAX_RECORDS) % MAX_RECORDS;
                        iterate = false;
                    }
                    else
                    {
                        YCSBBenchmark benchmark(backend, sequential_startidx);
                        benchmark.run();
                        total_hit_count += backend.hit_count;
                        sequential_startidx = (sequential_startidx + MAX_QUERIES) % MAX_RECORDS;
                        total_records_executed += benchmark.records_executed;
                    }

                });
            }
            for (auto& thread : threads) {
                thread.join();
            }
            BACKEND backend(0);
            backend.clean_up();
            hitrate = (double)total_hit_count/(double)total_records_executed;
            total_hit_count = 0;
            total_records_executed = 0;
            std::cout<<"last hit rate is: "<<lasthitrate<<" and hit rate is: "<<hitrate<<std::endl;
        }
        std::cout<<"warmup time is: "<<count<<std::endl;
    }

    if(do_run)
    {
        while(run_times --)
        {
            g_next_insert_key = MAX_RECORDS;
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; i++) {
                threads.emplace_back([cache_enabled, i, 
                                    &total_throughput, &total_usedtime, &total_hit_count, &total_records_executed,
                                    &total_latencies, &total_latencies_mutex, &sequential_startidx]() {
                    CachelibClient cacheclient;
                    cacheclient.addpool(UNIFIED_CACHE_POOL);
                    BACKEND backend(0);
                    if (cache_enabled) {
                        backend.enable_cache(cacheclient);
                    }
                    YCSBBenchmark benchmark(backend, sequential_startidx);
                    benchmark.run();
                    double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;
                    std::vector<unsigned int> latencies = benchmark.latencies_ns;
                    unsigned int hit_count = backend.hit_count;
                    unsigned int total_count = benchmark.records_executed;
                    // double hitrate = (double) hit_count / (double) total_count;
                    // unsigned int percentile_99 = percentile(latencies, 0.99);
                    // unsigned int percentile_95 = percentile(latencies, 0.95);
                    // unsigned int percentile_50 = percentile(latencies, 0.50);
                    // OUTPUT << "Thread " << i << " Hitrate: " << hitrate << std::endl;
                    // OUTPUT << "Thread " << i << " Throughput: " << throughput << " records/s" << std::endl;
                    // OUTPUT << "Thread " << i << " UsedTime: " << (double) benchmark.millis_elapsed/1000 << " s" << std::endl;
                    // OUTPUT << "Thread " << i << " 99th percentile: " << percentile_99 << " ns" << std::endl;
                    // OUTPUT << "Thread " << i << " 95th percentile: " << percentile_95 << " ns" << std::endl;
                    // OUTPUT << "Thread " << i << " 50th percentile: " << percentile_50 << " ns" << std::endl;

                    // aggregate the results
                    total_throughput += throughput;
                    total_usedtime += benchmark.millis_elapsed;
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
            total_usedtime = total_usedtime/num_threads;
            OUTPUT << "Total Hitrate: " << total_hitrate << std::endl;
            OUTPUT << "Total Throughput: " << total_throughput << " records/s" << std::endl;
            OUTPUT << "Total 99th percentile: " << total_percentile_99 << " ns" << std::endl;
            OUTPUT << "Total 95th percentile: " << total_percentile_95 << " ns" << std::endl;
            OUTPUT << "Total 50th percentile: " << total_percentile_50 << " ns" << std::endl;
            OUTPUT << "Total average latency: " << avarage_percentile << " ns" << std::endl;
            OUTPUT << "Total Used Time: " << total_usedtime << " ms" << std::endl;

            if (!profile_file.empty()) {
                std::ofstream out(profile_file + "_tailLatency.log", std::ios::app);
                out << total_percentile_99 << " " << total_percentile_95 << " " << total_percentile_50 << " " << total_hitrate << std::endl;
            }
            total_throughput = 0;
            total_usedtime = 0;
            total_hit_count = 0;
            total_records_executed = 0;
            total_latencies.clear();
            total_latencies.shrink_to_fit();
            sequential_startidx = (sequential_startidx + MAX_QUERIES) % MAX_RECORDS;
        }
    }
    return 0;
}
