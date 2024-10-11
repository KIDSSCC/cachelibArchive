#include "cachelib/allocator/CacheAllocator.h"
#include "folly/init/Init.h"

#include<iostream>

namespace facebook {
namespace cachelib_examples {

using Cache = cachelib::LruAllocator; // or Lru2QAllocator, or TinyLFUAllocator
using CacheConfig = typename Cache::Config;
using CacheKey = typename Cache::Key;
using CacheReadHandle = typename Cache::ReadHandle;
using CacheWriteHandle = typename Cache::WriteHandle;
using PoolId = cachelib::PoolId;
using PoolStats = cachelib::PoolStats;

// Global cache object and a default cache pool
std::unique_ptr<Cache> gCache_;
cachelib::PoolId defaultPool_;

//default cache pool size and Cache size
size_t cacheSize = (size_t)512*1024*1024 + (size_t)4*1024*1024;
size_t poolSize = (size_t)256*1024*1024;

void initializeCache() {
  CacheConfig config;
  config
      .setCacheSize(cacheSize) 
      .setCacheName("TestCacheLib")
      .setAccessConfig(
          {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
                                                        // million items
      .validate(); // will throw if bad config
  gCache_ = std::make_unique<Cache>(config);
  defaultPool_ = gCache_->addPool("default", poolSize);
}

void destroyCache() { gCache_.reset(); }

bool set(PoolId pid, CacheKey key, const std::string& value)
{
    CacheWriteHandle wh = gCache_->allocate(pid, key, value.size());
    if(!wh)
        return false;
    std::memcpy(wh->getMemory(), value.data(), value.size());
    gCache_->insertOrReplace(wh);
    return true;
}

std::string get(CacheKey key)
{
    CacheReadHandle rh = gCache_->find(key);
    if(!rh)
        return "";
    folly::StringPiece data{reinterpret_cast<const char*>(rh->getMemory()), rh->getSize()};
    return std::string(data);
}

}
}

using namespace facebook::cachelib_examples;

int main(int argc, char**argv)
{
    std::cout<<"Hello ,Cachelib\n";
    folly::Init init(&argc, &argv);
    initializeCache();

    //set test
    int success = 0;
    for(int i=0;i<10;i++)
    {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        bool res = set(defaultPool_, key, value);
        if(res)
            success++;
    }
    std::cout<<"finish set, success: "<<success<<std::endl;

    //get test
    success = 0;
    for(int i=0;i<10;i++)
    {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        std::string getValue = get(key);
        if(getValue == value)
            success++;
    }
    std::cout<<"finish get, success: "<<success<<std::endl;

    destroyCache();
}