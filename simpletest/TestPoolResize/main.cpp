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
cachelib::PoolId pool1_;
cachelib::PoolId pool2_;

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
  pool1_ = gCache_->addPool("pool1", poolSize);
  pool2_ = gCache_->addPool("pool2", poolSize);
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

void checkStat()
{
    std::set<PoolId> allpool = gCache_->getPoolIds();
    std::set<PoolId>::iterator itr=allpool.begin();
    for(itr; itr!=allpool.end();itr++)
    {
        PoolStats poolStat = gCache_->getPoolStats(*itr);
        std::cout<<"pool name is: "<< poolStat.poolName << std::endl;
        std::cout<<"\tpool size is: "<< poolStat.poolSize <<std::endl;
        std::cout<<"\tallocate attempts is: "<< poolStat.numAllocAttempts() <<std::endl;
        std::cout<<"\tfreeMemoryBytes is: "<< poolStat.freeMemoryBytes() <<std::endl;
        std::cout<<"\tnumItems is: "<< poolStat.numItems() <<std::endl;
    }

}

}
}

using namespace facebook::cachelib_examples;

int main(int argc, char**argv)
{
    std::cout<<"Hello ,Cachelib\n";
    folly::Init init(&argc, &argv);
    initializeCache();

    std::string key = "key_";
    std::string value_1 = "Hello,World!";
    std::string value_2 = "this is a value";

    set(pool1_, key, value_1);
    checkStat();
    std::cout<<"------------------------------------------------------\n";
    set(pool2_, key, value_2);
    checkStat();
    std::string getValue = get(key);
    std::cout<<"getValue is: "<<getValue<<std::endl;

    

    destroyCache();
}