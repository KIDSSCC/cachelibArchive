#pragma once

#include "common.h"
#include "benchmarks/config.h"
#include "cache/AutoIncCache.h"
#include <atomic>

#define DEFAULT_SUBPOOL_NAME "default"

// unified interface for all database backends
class Backend
{
public:
    Backend(int thread_id) : thread_id(thread_id) {}
    virtual ~Backend() = default;
    virtual bool create_database() = 0; // create a database with a unique name
    virtual void enable_cache(CachelibClient &client);
    virtual bool read_record(int key, std::vector<std::string>& results) = 0; // read a record from the database
    virtual bool insert_record(int key, std::vector<std::string>& values) = 0; // insert a record into the database
    virtual bool clean_up() = 0;

public:
    AutoIncCache cache; // cache for the database
    int thread_id = -1; // thread id for multi-threading
    std::string subpool_name = DEFAULT_SUBPOOL_NAME; // subpool name for cache
    bool cache_enabled = false; // whether cache is enabled

    unsigned int hit_count = 0; // number of cache hits
};