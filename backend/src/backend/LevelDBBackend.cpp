#include "LevelDBBackend.h"
#include "utils/cache_utils.h"
#include <leveldb/write_batch.h>
#include <leveldb/status.h>
#include <leveldb/cache.h>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <sys/stat.h>

LevelDBBackend::LevelDBBackend(int thread_id) : Backend(thread_id) {
    // check if dir /leveldb exists
    std::string dir = LEVELDB_DIR;
    struct stat info;
    if (stat(dir.c_str(), &info) != 0) {
        std::string command = "mkdir " + dir;
        if (system(command.c_str()) != 0) {
            throw std::runtime_error("Failed to create directory " + dir);
        }
    }
    try {
        db_path = LEVELDB_DIR "/db" + std::to_string(thread_id);
        options.create_if_missing = true;  // uutomatically create the database if it doesn't exist.
        options.block_cache = leveldb::NewLRUCache(0); // shrink down the leveldb's built-in cache
        leveldb::Status status = leveldb::DB::Open(options, db_path, &db);
        if (!status.ok()) {
            std::stringstream ss;
            ss << "Failed to open database: " << status.ToString();
            throw std::runtime_error(ss.str());
        }
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss << "Failed to open database: " << e.what();
        throw std::runtime_error(ss.str());
    }
}

LevelDBBackend::~LevelDBBackend() {
    delete db;  // Clean up the database on destruction.
    delete options.block_cache; // Clean up the cache on destruction.
}

bool LevelDBBackend::create_database() {
    return true;
}

bool LevelDBBackend::read_record(int key, std::vector<std::string>& results) {
    std::string value;
    if (cache_enabled) {
        // if cache is enabled, try to read from cache first
        // debug(
        //     auto start = std::chrono::high_resolution_clock::now();
        // );
        std::string value = cache.get_(std::to_string(key));
        // debug(
        //     auto end = std::chrono::high_resolution_clock::now();
        // );
        if (value != "") {
            results = unpack_values(value, MAX_FIELD_SIZE);
            // debug(
            //     total_cache_query_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                hit_count++;
            //     read_hit_count++;
            //     std::cout << "LevelDBBackend: cache hit: " << hit_count << std::endl;
            // );
            return true;
        }
    }
    // debug(
    //     auto start = std::chrono::high_resolution_clock::now();
    // );
    leveldb::ReadOptions options;
    // dont use leveldb's built-in cache, use our own cache
    // options.fill_cache = false;
    leveldb::Status s = db->Get(options, std::to_string(key), &value);
    // debug(
    //     auto end = std::chrono::high_resolution_clock::now();
    // );
    if (s.ok()) {
        results = unpack_values(value, MAX_FIELD_SIZE);
        // debug(
        //     read_miss_count++;
        //     total_leveldb_query_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        // );
        if (cache_enabled) {
            // debug(
            //     auto start = std::chrono::high_resolution_clock::now();
            // );
            if (!cache.set_(std::to_string(key), value)) {
                std::cout << "LevelDBBackend: cache set failed" << std::endl;
            }
            // debug(
            //     auto end = std::chrono::high_resolution_clock::now();
            //     total_write_cache_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // );
        }
        return true;
    }
    return false;
}

bool LevelDBBackend::insert_record(int key, std::vector<std::string>& values) {
    std::string value = pack_values(values);
    debug(
        insert_count++;
        // auto start = std::chrono::high_resolution_clock::now();
    );
    leveldb::Status s = db->Put(leveldb::WriteOptions(), std::to_string(key), value);
    // debug(
    //     auto end = std::chrono::high_resolution_clock::now();
    //     total_leveldb_insert_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // );
    if (cache_enabled) {
        // debug(
        //     auto start = std::chrono::high_resolution_clock::now();
        // );
        cache.set_(std::to_string(key), value);
        // debug(
        //     auto end = std::chrono::high_resolution_clock::now();
        //     total_write_cache_insert_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        // );
    }
    return s.ok();
}

bool LevelDBBackend::clean_up() {
    // remove all newly inserted records >= MAX_RECORDS
    for (int i = MAX_RECORDS; i < g_next_insert_key; i++) {
        db->Delete(leveldb::WriteOptions(), std::to_string(i));
    }
    return true;
}
