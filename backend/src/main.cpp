#include "common.h"
#include "benchmarks/YCSBBenchmark.h"
#include "benchmarks/DynamicBenchmark.h"
// 客户端程序
#include "backend/LevelDBBackend.h"
#include "backend/TmdbBackend.h"

#include "utils/percentile.h"
#include "utils/save_vector.h"
#include "generator/generator.h"
#include "clientAPI.h"

#include CONFIG_FILE

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <climits>
#include <chrono>

#define RUNTIME 600

bool cache_enabled = false;
bool do_prepare = true;
bool do_run = true;
int num_threads = 1;
int run_times = 0;
int choosed_workload = 0;
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
            do_run = false;
        } else if (arg == "--run") {
            do_prepare = false;
            if (i + 1 < argc) {
                run_times = std::stoi(argv[i + 1]);
            }
			// kidsscc: when args is -1, run the bench infinitely
			if(run_times==-1){
				run_times = INT_MAX;
			}
        }
    }
    return true;
}

int main(int argc, char* argv[]){
    // 命令行参数解析
    if(!arg_parser(argc, argv))
        return 0;

    // 工作负载仅有uniform一种
    std::vector<std::shared_ptr<Generator>> generators = {
        WORKLOAD_TYPE
    };

    atomic<double> total_throughput(0.0);
    atomic<double> total_usedtime(0.0);
    atomic<unsigned int> total_hit_count(0);
    atomic<unsigned int> total_records_executed(0);
    std::vector<unsigned int> total_latencies(num_threads * currentMaxQueries, 0);

    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
        if(cache_enabled){
            CachelibClient unified_cache;
            unified_cache.addpool(UNIFIED_CACHE_POOL);
            backend.enable_cache(unified_cache);
        }
        // prepare阶段，传入benchmark的query生成器类型无所谓，都是按sequential执行数据存储，但是其中工作集大小需要确认
        DynamicBenchmark benchmark(backend, generators[0]);
        benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted." << std::endl;
        return 0;
    }

    // 从一个选定的query生成器开始执行，默认为1，第一阶段开始warmup
    int generator_idx = choosed_workload;
    long long threshold = RUNTIME;
    auto start_time = std::chrono::system_clock::now();
    auto end_time = std::chrono::system_clock::now();

    //开始执行
    while(do_run && run_times>0){
        run_times--;
        // 创建多线程执行查询任务
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([ i, &total_throughput, &total_usedtime, &total_hit_count, 
                            &total_records_executed, &total_latencies,
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
                if(generator_idx != 0)
                {
                    // 替换为无锁结构
                    for(size_t idx = 0; idx < benchmark.latencies_ns.size();idx++)
                    {
                        total_latencies[idx + currentMaxQueries * i] = benchmark.latencies_ns[idx];
                    }
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }

        BACKEND backend(0);
        backend.clean_up();

        // 统计数据汇总
        unsigned int average_percentile = 0;
        unsigned int total_percentile_99 = 0;
        average_and_percentile(total_latencies, &average_percentile, &total_percentile_99);
        double total_hitrate = (double) total_hit_count / (double) total_records_executed;
        total_usedtime = total_usedtime/num_threads;

        std::cout << total_percentile_99 << " " 
                    << average_percentile << " " 
                    << total_usedtime << " "
                    << total_throughput << " " 
                    << total_hitrate << std::endl;

        // 数据清理
        total_throughput = 0;
        total_usedtime = 0;
        total_hit_count = 0;
        total_records_executed = 0;

        //动态负载控制
        end_time = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        if(duration >= threshold) {
            break;
        }
    }
    
    return 0;
}
