#include "DynamicBenchmark.h"
#include <thread>
#include <memory>

/**
 * DynamicBenchmark：构造函数，包含所使用的后端，query所用的生成器类型，与要进行的查询次数
 * params：
 *      backend：所使用的数据库后端
 *      generator：进行query时采样分布
 *      curr_query：本轮次要进行的query次数
 */
DynamicBenchmark::DynamicBenchmark(Backend& backend, std::shared_ptr<Generator>& generator, unsigned int curr_query)
    : Benchmark(backend) { 
        init(generator, curr_query);
}

/**
 * init：对benchmark实例进行初始化
 * params：
 *      generator：进行query时的采样分布
 *      curr_query：本轮次要进行的query次数
 */
void DynamicBenchmark::init(std::shared_ptr<Generator>& generator, unsigned int curr_query) {
    max_query = curr_query;
    latencies_ns.reserve(max_query);

    name = "Dynamic";

    // 生成线程与时间相关的随机数种子，用于生成query编号
    std::thread::id thread_id = std::this_thread::get_id();
    std::hash<std::thread::id> hash_func;
    size_t hashed_id = hash_func(thread_id);
    size_t seed = static_cast<size_t>(std::time(0))
                    ^ (hashed_id + 0x9e3779b9
                    + (static_cast<size_t>(std::time(0)) << 6)
                    + (static_cast<size_t>(std::time(0)) >> 2));
    random_engine = std::default_random_engine(seed);
    generate_ = generator;
    rng = std::mt19937(rd());
    
}

/**
 * create_database：调用backend的create_database进行数据库的创建，返回创建结果
 * return：
 *      bool：创建是否成功
 */
bool DynamicBenchmark::create_database() {
    debug(
        std::cout << "DynamicBenchmark: creating database" << std::endl;
    );
    return backend.create_database();
}

/**
 * load_database：向数据库中写入max_records条数据，用于后续查询
 * return：
 *      bool：写入是否成功
 */
bool DynamicBenchmark::load_database() {
    debug(
        std::cout << "DynamicBenchmark: loading database" << std::endl;
    );
    // insert records into the database
    for (unsigned int i = 0; i < max_records; i++) {
        std::vector<std::string> values;
        for (unsigned int j = 0; j < max_fields; j++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!backend.insert_record(i, values)) {
            std::cout << "DynamicBenchmark: load_database: insert record failed" << std::endl;
            return false;
        }
    }
    g_next_insert_key = max_records;
    return true;
}

/**
 * step：在数据库中进行单步查询或插入操作
 * return：
 *      bool：查询或插入结果是否成功
 */
bool DynamicBenchmark::step() {
    // excute different queries based on the proportion
    if (rng() % 100 < query_proportion * 100) {
        // read query
        std::vector<std::string> results;
        int key = generate_->get_num(random_engine);
        if (!read_record(key, results)) {
            std::cout << "DynamicBenchmark: read query failed" << std::endl;
            return false;
        }
    } else {
        //insert operation
        std::vector<std::string> values;
        for (unsigned int i = 0; i < max_fields; i++) {
            values.emplace_back(randomFastString(rng, max_field_size));
        }
        if (!insert_record(g_next_insert_key++, values)) {
            std::cout << "DynamicBenchmark: insert query failed" << std::endl;
            return false;
        }
    }
    current_query++;
    return true;
}

/**
 * read_record：根据key从数据库中进行查询获取value
 * params:
 *      key:待查询的key的编号
 *      result：传递查询结果
 * return：
 *      bool：查询结果是否成功
 */
bool DynamicBenchmark::read_record(int key, std::vector<std::string>& results) {
    if (!backend.read_record(key, results)) {
        std::cout << "DynamicBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    // make sure results are not optimized away
    if (results.size() == 0) {
        std::cout << "DynamicBenchmark: read_record: " << key << " failed" << std::endl;
        return false;
    }
    return true;
}

/**
 * insert_record：根据key，value向数据库中插入新的记录
 * params：
 *      key：新插入的数据项编号
 *      values：新插入的数据项
 * return：
 *      bool：插入结果是否成功
 */
bool DynamicBenchmark::insert_record(int key, std::vector<std::string>& values) {
    if (!backend.insert_record(key, values)) {
        std::cout << "DynamicBenchmark: insert_record: " << key << " failed" << std::endl;
        return false;
    }

    /**
     * 此处仅调整了benchmark实例中记录的max_records，即数据中内完整数据项的数目
     * 对后续的查询过程不会产生影响。generator中记录的工作集大小由generator自身进行决定。
     * 此处并未对generator进行调整。
     */
    max_records++;
    return true;
}

/**
 * is_end：检查当前轮次query是否结束。在max_query为0时为持续进行
 * return：
 *      bool：是否为持续query或query次数已达到max_query
 */
bool DynamicBenchmark::is_end() {
    if(max_query==0)
        return false;
    return current_query >= max_query;
}

/**
 * cleanup:benchmark内不需要进行清理工作
 */
bool DynamicBenchmark::cleanup() {
    return true;
}
