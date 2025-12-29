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

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <climits>
#include <chrono>

#define WARMUP_THRESHOLD 0.005
#define WARMTIME 120
#define RUNTIME 600

bool cache_enabled = false;
bool do_prepare = false;
bool do_warmup = true;
bool do_run = true;
int num_threads = 1;
int choosed_workload = 0;
std::string profile_file = string("bin_") + UNIFIED_CACHE_POOL;
int logInfo = -1;
int currentMaxQueries = MAX_QUERIES;
// final_eof初始化为true时，将避免最后额外的一轮执行
bool final_eof = true;

std::atomic<int> g_next_insert_key;

/**
 * arg_parser:命令行参数解析
 * return：
 *      bool：是否解析得到有效参数
 */
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
            do_prepare = true;
            do_warmup = false;
            do_run = false;
        } else if (arg == "--warmup") {
            do_prepare = false;
			do_warmup = true;
        } else if (arg == "--run") {
            do_prepare = false;
            do_run = true;
        } else if (arg == "--profile") {
            if (i + 1 < argc) {
                profile_file = argv[i + 1];
            }
		}else if(arg=="--loginfo"){
			if(i + 1 < argc){
				logInfo = std::stoi(argv[i+1]);
			}
		}else if(arg=="--maxquery"){
			if(i + 1 < argc){
				currentMaxQueries = std::stoi(argv[i+1]);
			}
        }else if(arg == "--workload"){
            if(i + 1 < argc){
				choosed_workload = std::stoi(argv[i+1]);
			}
        } else if (arg == "--help") {
            std::cout << "Usage: sqlcache [options]\n"
                      << "Options:\n"
                      << "  --cache: enable cache\n"
                      << "  --threads <num>: number of threads\n"
                      << "  --prepare: only prepare the database\n"
                      << "  --warmup: run the benchmark continuously for WARMTIME as a warm-up phase.\n"
                      << "  --run: run the benchmark continuously for RUNTIME * sizeof(generator)\n"
                      << "  --profile <file>: dump the latencies to a file\n"
                      << "  --loginfo <file>: dump a particular metric individually to a file\n"
                      << "  --maxquery <num>: dump logs every <num> querys\n"
                      << "  --workload <num>: start benchmark at <num>-th generator\n"
                      << "  --help: show this message\n";
            return false;
        }
    }
    return true;
}

void log_output(
    unsigned int& total_percentile_99, 
    unsigned int& average_percentile,
    atomic<double>& total_usedtime,
    atomic<double>& total_throughput,
    double& total_hitrate
){
    // 输出完整元日志
    if (!profile_file.empty()) {
        std::ofstream out(profile_file + "_meta.log", std::ios::app);
        out << total_percentile_99 << " " 
            << average_percentile << " " 
            << total_usedtime << " "
            << total_throughput << " " 
            << total_hitrate << std::endl;
    }
    if(!profile_file.empty() && logInfo !=-1) {
        std::ofstream out(profile_file + "_subItem.log", std::ios::app);
        switch(logInfo){
            case 0:
                out << total_percentile_99 << std::endl;
                break;
            case 1:
                out << average_percentile << std::endl;
                break;
            case 2:
                out << total_usedtime << std::endl;
                break;
            case 3:
                out << total_throughput << std::endl;
                break;
            case 4:
                out << total_hitrate << std::endl;
                break;
            default:break;
        }
    }
}

int main(int argc, char* argv[]){
    // 命令行参数解析
    if(!arg_parser(argc, argv))
        return 0;

    // 利用配置文件构造query生成器，使用20%大小的工作集进行最初的warmup
    std::vector<std::shared_ptr<Generator>> generators = {
        WORKLOAD_TYPE
    };
    
    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
        //kidsscc:write to cache in prepare phase
        CachelibClient unified_cache;
        if(cache_enabled){
            unified_cache.addpool(UNIFIED_CACHE_POOL);
            backend.enable_cache(unified_cache);
        }
        // prepare阶段，传入benchmark的query生成器类型无所谓，都是按sequential执行数据存储，但是其中工作集大小需要确认
        DynamicBenchmark benchmark(backend, generators[0]);
        unsigned int usedTime = benchmark.prepare();
        std::cout << "Preparation done, " << g_next_insert_key << " records inserted" << std::endl;
        std::cout << "Prepare Used Time: " << usedTime << std::endl;
        return 0;
    }

    atomic<double> total_throughput(0.0);
    atomic<double> total_usedtime(0.0);
    atomic<unsigned int> total_hit_count(0);
    atomic<unsigned int> total_records_executed(0);
    std::vector<unsigned int> total_latencies(num_threads * currentMaxQueries, 0);

    bool warmup_finish = !do_warmup;
    [[maybe_unused]] double last_hitrate = -1.0;

    long long warm_threshold = WARMTIME;
    auto warm_start_time = std::chrono::system_clock::now();
    auto warm_end_time = std::chrono::system_clock::now();
    while(!warmup_finish){
        // std::shared_ptr<Generator> warmup_generator = std::make_shared<Generator>(D_UNIFORM, generators[0]->get_max(), std::vector<double>{});
        std::shared_ptr<Generator> warmup_generator = generators[0];
        std::vector<std::thread> threads;
        for(int i=0;i<num_threads;i++){
            threads.emplace_back([i, &total_throughput, &total_usedtime, &total_hit_count,
                            &total_records_executed, &total_latencies, &warmup_generator](){
                CachelibClient cacheclient;
                BACKEND backend(i);
                if (cache_enabled) {
                    cacheclient.addpool(UNIFIED_CACHE_POOL);
                    backend.enable_cache(cacheclient);
                }
                int adjust_querys = currentMaxQueries;
                /**
                 * 在warmup阶段，由于冷启动的原因，每一批次执行完成的时间可能较长
                 * 为了避免超出warmup时间限制，对warmup阶段每一批次执行的query进行调整。
                 * 使其能更快速的完成每一轮次，执行时间检查
                 */
                // adjust_querys = 500;

                DynamicBenchmark benchmark(backend, warmup_generator, adjust_querys);
                benchmark.run();

                double throughput = (double) benchmark.records_executed / (double) benchmark.millis_elapsed * 1000;

                // aggregate the results
                total_throughput = total_throughput + throughput;
                total_usedtime = total_usedtime + benchmark.millis_elapsed;
                total_hit_count += backend.hit_count;
                total_records_executed += benchmark.records_executed;

                total_latencies.assign(total_latencies.size(), 0);
                std::copy(benchmark.latencies_ns.begin(), benchmark.latencies_ns.end(), total_latencies.begin());

            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        BACKEND backend(0);
        backend.clean_up();

        // 统计数据汇总
        unsigned int total_percentile_99 = 0;
        unsigned int average_percentile = 0;
        average_and_percentile(total_latencies, &average_percentile, &total_percentile_99);
        double total_hitrate = (double) total_hit_count / (double) total_records_executed;
        total_usedtime = total_usedtime/num_threads;

        log_output(total_percentile_99, average_percentile, total_usedtime, total_throughput, total_hitrate);

        // 数据清理
        total_throughput = 0;
        total_usedtime = 0;
        total_hit_count = 0;
        total_records_executed = 0;

        // warmup 终止, 两轮执行的命中率之差小于阈值
        // if(last_hitrate<0 || (last_hitrate>0 && abs(total_hitrate - last_hitrate)>WARMUP_THRESHOLD)){
        //     last_hitrate = total_hitrate;
        // }else{
        //     cout<<"Workload warmup finished\n";
        //     warmup_finish = true;
        // }

        // warmup终止，固定时长warmup
        warm_end_time = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(warm_end_time - warm_start_time).count();
        if(duration >= warm_threshold){
            cout<<"Workload warmup finished\n";
            warmup_finish = true;
        }
    }

    // 从一个选定的query生成器开始执行，默认为0
    int generator_idx = choosed_workload;
    long long threshold = RUNTIME;
    auto start_time = std::chrono::system_clock::now();
    auto end_time = std::chrono::system_clock::now();

    //开始执行
    while(do_run){
        // 创建多线程执行查询任务
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([ i, &total_throughput, &total_usedtime, &total_hit_count, 
                    &total_records_executed, &total_latencies, &generators, &generator_idx]() {
                CachelibClient cacheclient;
                BACKEND backend(i);
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
                // 替换为无锁结构
                for(size_t idx = 0; idx < benchmark.latencies_ns.size();idx++)
                {
                    total_latencies[idx + currentMaxQueries * i] = benchmark.latencies_ns[idx];
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

        log_output(total_percentile_99, average_percentile, total_usedtime, total_throughput, total_hitrate);
        // 数据清理
        total_throughput = 0;
        total_usedtime = 0;
        total_hit_count = 0;
        total_records_executed = 0;

        //动态负载控制
        end_time = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

        // 负载变化
        if(duration >= threshold) {
            start_time = std::chrono::system_clock::now();
            generator_idx++;

            // 当所有query生成器均用完时，不再进行负载变换，仅按照最后一个query生成器继续执行一轮
            if(generator_idx >= (int)generators.size()){
                // 标识目前已经进入最后一轮
                if(!final_eof){
                    generator_idx = generators.size() - 1;
                    final_eof = true;
                }else{
                    break;
                }
            }            
        }
    }
    
    return 0;
}
