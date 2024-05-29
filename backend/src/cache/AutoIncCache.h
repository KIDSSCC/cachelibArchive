#pragma once 
#include "clientAPI.h"
#include <atomic>
#include <string>
#include <queue>
#include <unordered_map>
#include "common.h"

class AutoIncCache
{
private:
    std::atomic<int> counter{0};
    std::string subpool_name;
    CachelibClient* client;

public:
    AutoIncCache() = default;
    ~AutoIncCache() = default;
    void initialize(CachelibClient &client);
    void destroy();
    void set_subpool(std::string subpool);
    bool set_(const std::string& key, const std::string& value);
    std::string get_(const std::string& key);
    bool del_(const std::string& key);
    void invalidate_all_();
};
