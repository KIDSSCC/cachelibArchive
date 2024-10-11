#include "cachelib/allocator/CacheAllocator.h"
#include "folly/init/Init.h"

#include<iostream>
#define ENABLE_RESIZE 1

namespace facebook {
namespace cachelib_examples {

using Cache = cachelib::LruAllocator; // or Lru2QAllocator, or TinyLFUAllocator
using CacheConfig = typename Cache::Config;
using CacheKey = typename Cache::Key;
using CacheReadHandle = typename Cache::ReadHandle;
using CacheWriteHandle = typename Cache::WriteHandle;
using PoolId = cachelib::PoolId;
using PoolStats = cachelib::PoolStats;
using RebalanceStrategy=cachelib::RebalanceStrategy;

// Global cache object and a default cache pool
std::unique_ptr<Cache> gCache_;
cachelib::PoolId pool1_;
cachelib::PoolId pool2_;

//default cache pool size and Cache size
size_t cacheSize = (size_t)512*1024*1024 + (size_t)4*1024*1024;
size_t poolSize = 0;
size_t itemSize = (size_t)3 * 1024 * 1024;
size_t adjustSize = (size_t)128 *1024 * 1024;

void initializeCache() {
    CacheConfig config;
    config
        .setCacheSize(cacheSize) 
        .setCacheName("TestCacheLib")
        .setAccessConfig(
            {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
                                                        // million items
        .validate(); // will throw if bad config

    #if ENABLE_RESIZE
        const uint32_t poolResizeSlabsPerIter = 100000;
        config.enablePoolResizing(std::make_shared<RebalanceStrategy>(),std::chrono::milliseconds{1}, poolResizeSlabsPerIter);
        std::cout<<"**********enable pool resize**********\n";
    #else
        std::cout<<"**********no pool resize**********\n";
    #endif
    gCache_ = std::make_unique<Cache>(config);
}

void destroyCache() { gCache_.reset(); }

bool set(PoolId pid, CacheKey key, const std::string& value)
{
    CacheWriteHandle wh = gCache_->allocate(pid, key, itemSize);
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
        std::cout<<"\tpool size is: "<< poolStat.poolSize/(1024*1024)<<" MB" <<std::endl;
        std::cout<<"\tallocate attempts is: "<< poolStat.numAllocAttempts() <<std::endl;
        std::cout<<"\tfreeMemoryBytes is: "<< poolStat.freeMemoryBytes() <<std::endl;
        std::cout<<"\tnumItems is: "<< poolStat.numItems() <<std::endl;
    }
    std::cout<<"-----------------------------------------------------\n";
}

}
}

using namespace facebook::cachelib_examples;

int main(int argc, char**argv)
{
    std::cout<<"Hello ,Cachelib\n";
    folly::Init init(&argc, &argv);
    initializeCache();

    std::cout<<"**********add two 256MB pool**********\n";
    pool1_ = gCache_->addPool("pool1", poolSize);
    pool2_ = gCache_->addPool("pool2", poolSize);
    checkStat();

    std::cout<<"**********set 4MB * 64 data in pool1**********\n";
    for(int i=0;i<64;i++)
    {
        set(pool1_, "key" + std::to_string(i), "value");
    }
    checkStat();
    
    std::cout<<"**********pool resize, 128MB from pool1 to pool2**********\n";
    // auto res = gCache_->resizePools(pool2_, pool1_, adjustSize);
    auto res = gCache_->shrinkPool(pool1_, adjustSize);
    if(res)
        std::cout<<"resize success\n";
    else
        std::cout<<"resize failed\n";

    res = gCache_->growPool(pool2_, adjustSize);
    if(res)
        std::cout<<"resize success\n";
    else
        std::cout<<"resize failed\n";
    std::cout<<"**********attempt to set 4MB * 4992 data in pool2**********\n";
    for(int i=128;i<5120;i++)
    {
        set(pool2_, "key" + std::to_string(i), "value");
    }
    checkStat();

    
    destroyCache();
}