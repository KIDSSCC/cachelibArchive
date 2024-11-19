#include "common.h"
#include "benchmarks/YCSBBenchmark.h"
#include "benchmarks/DynamicBenchmark.h"
#include "backend/MysqlBackend.h"
#include "backend/MongoDBBackend.h"
#include "backend/LevelDBBackend.h"
#include "backend/SQLiteBackend.h"
#include "backend/TmdbBackend.h"
#include "utils/percentile.h"
#include "utils/save_vector.h"
#include "generator/generator.h"
#include "clientAPI.h"

#include CONFIG_FILE
// #include "config.h"

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <climits>
#include <chrono>

#define WARMTIME 300
#define RUNTIME 600

bool cache_enabled = false;
bool do_prepare = true;
bool do_warmup = false;
bool do_run = true;
int num_threads = 1;
int run_times = 0;
int choosed_workload = 0;
std::string profile_file = "";
int logInfo = 0;
int currentMaxQueries = MAX_QUERIES;

std::atomic<int> g_next_insert_key;

bool arg_parser(int argc, char* argv[]){
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
			do_warmup = true;
        } else if (arg == "--run") {
            do_prepare = false;
            if (i + 1 < argc) {
                run_times = std::stoi(argv[i + 1]);
            }
			// kidsscc: when args is -1, run the bench infinitely
			if(run_times==-1){
				run_times = INT_MAX;
			}
        } else if (arg == "--profile") {
            if (i + 1 < argc) {
                profile_file = argv[i + 1];
            }
		}else if(arg=="--loginfo"){
			if(i + 1 < argc){
				logInfo = std::stoi(argv[i+1]);
			}
		}else if(arg=="--maxquery"){
			if(i+1<argc){
				currentMaxQueries = std::stoi(argv[i+1]);
			}
        }else if(arg == "--workload"){
            if(i+1<argc){
				choosed_workload = std::stoi(argv[i+1]);
			}
        } else if (arg == "--help") {
            std::cout << "Usage: sqlcache [options]\n"
                      << "Options:\n"
                      << "  --cache: enable cache\n"
                      << "  --threads <num>: number of threads\n"
                      << "  --prepare: only prepare the database\n"
                      << "  --warmup <num>: run the benchmark for <num> times as warmup\n"
                      << "  --run <num>: run the benchmark for <num> times\n"
                      << "  --profile <file>: dump the latencies to a file\n"
                      << "  --help: show this message\n"
                      << "if no options are provided, both preparation and benchmark will be run\n";
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]){
    if(!arg_parser(argc, argv))
        return 0;
    std::vector<std::shared_ptr<Generator>> generators = {
        WORKLOAD_TYPE
    };
    // kidsscc: 使用20%大小的工作集进行warmup
    generators.emplace(generators.begin(), std::make_shared<Generator>(D_UNIFORM, MAX_RECORDS * 0.2, std::vector<double>{}));  

    atomic<double> total_throughput = 0;
    atomic<double> total_usedtime = 0;
    atomic<unsigned int> total_hit_count = 0;
    atomic<unsigned int> total_records_executed = 0;
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
        DynamicBenchmark benchmark(backend, generators[0]);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
    }

    std::tm specific_time = {};
    specific_time.tm_year = 2024 - 1900; // 年份从1900开始
    specific_time.tm_mon = 11 - 1;         // 月份从0开始
    specific_time.tm_mday = 18;            // 日
    specific_time.tm_hour = 19;
    specific_time.tm_min = 0;
    specific_time.tm_sec = 0; 

    std::time_t specific_time_t = std::mktime(&specific_time);
    // // 转换为 time_point
    auto specific_time_point = std::chrono::system_clock::from_time_t(specific_time_t);


    int generator_idx = choosed_workload;
    long long threshold = WARMTIME;
    auto start_time = std::chrono::system_clock::now();
    auto end_time = std::chrono::system_clock::now();

    //开始执行
    while(do_run){
        std::ofstream outx(profile_file + "_subItem.log", std::ios::app);
        std::ofstream outy(profile_file + "_subItem2.log", std::ios::app);
        auto sub_start = std::chrono::system_clock::now();
        outx << "sub start time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_start - specific_time_point).count() << std::endl;
        outy << "sub start time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_start - specific_time_point).count() << std::endl;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([ &total_throughput, &total_usedtime, &total_hit_count, 
                            &total_records_executed, &total_latencies, &total_latencies_mutex,
                            &generators, &generator_idx]() {
                CachelibClient cacheclient;
                BACKEND backend(0);
                if (cache_enabled) {
                    cacheclient.addpool(UNIFIED_CACHE_POOL);
                    backend.enable_cache(cacheclient);
                }
                DynamicBenchmark benchmark(backend, generators[generator_idx], currentMaxQueries);
                benchmark.run();

                double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;

                // aggregate the results
                total_throughput = total_throughput + throughput;
                total_usedtime = total_usedtime + benchmark.millis_elapsed;
                total_hit_count += backend.hit_count;
                total_records_executed += benchmark.records_executed;
                {
                    std::lock_guard<std::mutex> lock(total_latencies_mutex);
                    total_latencies.insert(total_latencies.end(), benchmark.latencies_ns.begin(), benchmark.latencies_ns.end());
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }

        BACKEND backend(0);
        backend.clean_up();

        auto sub_end = std::chrono::system_clock::now();
        outx << "sub end time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_end - specific_time_point).count() << std::endl;
        outy << "sub end time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_end - specific_time_point).count() << std::endl;

        unsigned int average_percentile = 0;
        unsigned int total_percentile_99 = 0;
        average_and_percentile(total_latencies, &average_percentile, &total_percentile_99);
        double total_hitrate = (double) total_hit_count / (double) total_records_executed;
        total_usedtime = total_usedtime/num_threads;

        auto sub_sub_end = std::chrono::system_clock::now();
        outx << "sub sub end time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_sub_end - specific_time_point).count() << std::endl;
        outy << "sub sub end time is: " << std::chrono::duration_cast<std::chrono::seconds>(sub_sub_end - specific_time_point).count() << std::endl;


        if (!profile_file.empty()) {
            std::ofstream out(profile_file + "_meta.log", std::ios::app);
            out << total_percentile_99 << " " 
                << average_percentile << " " 
                << total_usedtime << " "
                << total_throughput << " " 
                << total_hitrate << std::endl;
        }

        if(!profile_file.empty()) {
            std::ofstream out(profile_file + "_subItem.log", std::ios::app);
            switch(logInfo){
                case 0:
                    out << total_percentile_99 << std::endl;
                    break;
                case 3:
                    out << average_percentile << std::endl;
                    break;
                case 4:
                    out << total_throughput << std::endl;
                    break;
                case 5:
                    out << total_hitrate << std::endl;
                    break;
                default:break;
            }
            //固定向日志输出命中率
            std::ofstream out2(profile_file + "_subItem2.log", std::ios::app);
            out2 << total_hitrate << std::endl;
        }


        total_throughput = 0;
        total_usedtime = 0;
        total_hit_count = 0;
        total_records_executed = 0;
        total_latencies.clear();
        total_latencies.shrink_to_fit();

        //动态负载控制
        end_time = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        outx << "start time is:" << std::chrono::duration_cast<std::chrono::seconds>(start_time - specific_time_point).count()
            << " end time is:" << std::chrono::duration_cast<std::chrono::seconds>(end_time - specific_time_point).count()
            << " duration is:" << duration<< std::endl;
        outy << "start time is:" << std::chrono::duration_cast<std::chrono::seconds>(start_time - specific_time_point).count()
            << " end time is:" << std::chrono::duration_cast<std::chrono::seconds>(end_time - specific_time_point).count()
            << " duration is:" << duration<< std::endl;
        if(duration >= threshold) {
            threshold = RUNTIME;
            start_time = std::chrono::system_clock::now();
            outx << "workload change, duration is:" << duration << " seconds, next phase is: " << threshold<< std::endl;
            outy << "workload change, duration is:" << duration << " seconds, next phase is: " << threshold<< std::endl;
            generator_idx++;
            if(generator_idx>=(int)generators.size()){
                break;
            }
        }
    }
    
}
