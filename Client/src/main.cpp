#include <iostream>
#include "ThreadPool/ThreadPool.h"
#include "backend/MysqlBackend.h"
#include "benchmarks/DynamicBenchmark.h"
#include "clientAPI.h"

// TODO: 替换成通用的模板头文件
#include "config/mysql_1.h"

using namespace std;

#define WARMUP_THRESHOLD 0.005
#define WARMTIME 120
#define RUNTIME 600

bool    cache_enabled = false;
bool    do_prepare = false;
bool    do_warmup = true;
bool    do_run = true;
int     num_threads = 1;
string  profile_file = "";
int     current_max_queries = 100000;

/**
 * arg_parser:命令行参数解析
 * return：
 *      bool：是否解析得到有效参数
 */
bool arg_parser(int argc, char* argv[]){
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
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
		}else if(arg=="--maxquery"){
			if(i + 1 < argc){
				current_max_queries = std::stoi(argv[i+1]);
			}
        }else if (arg == "--help") {
            std::cout << "Usage: sqlcache [options]\n"
                      << "Options:\n"
                      << "  --cache: enable cache\n"
                      << "  --threads <num>: number of threads\n"
                      << "  --prepare: only prepare the database\n"
                      << "  --warmup: run the benchmark continuously for WARMTIME as a warm-up phase.\n"
                      << "  --run: run the benchmark continuously for RUNTIME * sizeof(generator)\n"
                      << "  --profile <file>: dump the latencies to a file\n"
                      << "  --maxquery <num>: dump logs every <num> querys\n"
                      << "  --help: show this message\n";
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]){
    // 命令行参数解析
    if(!arg_parser(argc, argv))
        return 1;
    
    // 利用配置文件构造query生成器，使用20%大小的工作集进行最初的warmup
    std::vector<std::shared_ptr<Generator>> generators = {
        WORKLOAD_TYPE
    };

    if (do_prepare) {
        BACKEND backend(0); // therad_id = 0 for same table across threads
    
        // prepare阶段，传入benchmark的query生成器类型无所谓，都是按sequential执行数据存储，但是其中工作集大小需要确认
        DynamicBenchmark benchmark(backend, generators[0]);
        unsigned int usedTime = benchmark.prepare();
        std::cout << "Preparation done, " << generators[0]->get_max() << " records inserted" << std::endl;
        std::cout << "Prepare Used Time: " << usedTime << std::endl;
        return 0;
    }
}