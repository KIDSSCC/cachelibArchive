#ifndef CACHELIB_HEADER
#define CACHELIB_HEADER

#include "cachelib/allocator/CacheAllocator.h"
#include "cachelib/allocator/nvmcache/NavyConfig.h"
#include "folly/init/Init.h"



#include<string>
#include<set>
#include <config.h>

namespace facebook {
namespace cachelib_examples {
using Cache = cachelib::LruAllocator;
using CacheConfig = typename Cache::Config;
using CacheKey = typename Cache::Key;
using CacheReadHandle = typename Cache::ReadHandle;
using CacheWriteHandle = typename Cache::WriteHandle;
using PoolId = cachelib::PoolId;
using PoolStats = cachelib::PoolStats;
using CacheMemoryStats = cachelib::CacheMemoryStats;
using RemoveRes = Cache::RemoveRes;
using RebalanceStrategy=cachelib::RebalanceStrategy;

using Estimates = cachelib::util::PercentileStats::Estimates;
using NavyConfig = cachelib::navy::NavyConfig;


// Global cache object 
extern std::unique_ptr<Cache> gCache_;

//default cache pool size and Cache size
extern size_t cacheSize ;
extern size_t poolSize;


//set cachelib default config
void cacheConfigure(CacheConfig& config);

//ser hybrid config(nvm config)
NavyConfig getNvmConfig(const std::string& cacheDir);

//create a new cachelib instance
void initializeCache(size_t pool_size);

//reclaim cachelib instance
void destroyCache();

//add a new pool or search the existing pool,return pool id
int addpool_(std::string poolName);

//data access API
bool set_(cachelib::PoolId pid, CacheKey key, const std::string& value);
std::string get_(CacheKey key);
bool del_(CacheKey key);


//get total available size of the cache instance
size_t getAvailableSize();

//get pool id of all pools
std::set<PoolId> getPoolIds_();

//get pool status according pool id
PoolStats getPoolStat(PoolId pid);

//get pool size according pool name
size_t getPoolSizeFromName(std::string poolName);

//adjust the pool size
void resizePool(std::string poolName, size_t newSize);

}
}

#endif // CACHELIB_HEADER
