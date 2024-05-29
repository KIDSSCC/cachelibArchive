#pragma once

#include "backend.h"
#include <leveldb/db.h>
#include <string>
#include <vector>
#include <mutex>

#define LEVELDB_DIR "./leveldb"

class LevelDBBackend : public Backend {
private:
    leveldb::DB* db;
    std::mutex mtx;
    std::string db_path = LEVELDB_DIR "/db";
    leveldb::Options options;

public:
    LevelDBBackend(int thread_id);
    ~LevelDBBackend() override;

    bool create_database() override;
    bool read_record(int key, std::vector<std::string>& results) override;
    bool insert_record(int key, std::vector<std::string>& values) override;
    bool clean_up() override;

    unsigned int read_hit_count = 0;
    unsigned int read_miss_count = 0;
    unsigned int insert_count = 0;
    unsigned int total_cache_query_latency_ns = 0;
    unsigned int total_leveldb_query_latency_ns = 0;
    unsigned int total_write_cache_latency_ns = 0;
    unsigned int total_leveldb_insert_latency_ns = 0;
    unsigned int total_write_cache_insert_latency_ns = 0;
};
