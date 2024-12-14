#include "benchmark.h"

bool Benchmark::is_end() {
    return true;
}

void Benchmark::prepare() {
    create_database();
    load_database();
}

/**
 * @brief 执行一个批次的查询请求
 * 
 * 执行一个批次的查询请求，记录每一条查询请求的完成延时
 * 
 * @return 
 */
void Benchmark::run() {
    auto start = std::chrono::high_resolution_clock::now();
    while (!is_end()) {
        auto start_step = std::chrono::high_resolution_clock::now();
        bool ret = step();
        auto end_step = std::chrono::high_resolution_clock::now();

        // 保存查询延时
        auto query_latency = std::chrono::duration_cast<std::chrono::microseconds>(end_step - start_step).count();
        latencies_ns.push_back(query_latency);

        if (!ret) {
            std::cout << "Benchmark: step failed" << std::endl;
            throw std::runtime_error("Benchmark: step failed");
        } 
        records_executed++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    millis_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    cleanup();
}