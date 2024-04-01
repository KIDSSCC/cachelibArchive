/*
    cachelib单独的接口库
*/

#ifndef CACHELIB_HEADER
#define CACHELIB_HEADER

#include "cachelib/allocator/CacheAllocator.h"
#include "folly/init/Init.h"


#include<string>
#include<set>

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


// Global cache object 
extern std::unique_ptr<Cache> gCache_;

//default cache pool size and Cache size
extern size_t cacheSize ;
extern size_t poolSize;


//提供创建cachelib实例时默认的配置
void cacheConfigure(CacheConfig& config);

//对cachelib实例的初始化并启动cachelib实例
void initializeCache();

//销毁cachelib实例
void destroyCache();

//向cachelib中添加一个新的池或搜索现有的池，池的大小为默认的参数
int addpool_(std::string poolName);

//数据set操作
bool set_(cachelib::PoolId pid, CacheKey key, const std::string& value);

//数据get操作
std::string get_(CacheKey key);

//数据del操作
bool del_(CacheKey key);


//cachelib stats
size_t getAvailableSize();
std::set<PoolId> getPoolIds_();
PoolStats getPoolStat(PoolId pid);
size_t getPoolSizeFromName(std::string poolName);
void resizePool(std::string poolName, size_t newSize);

}
}

#endif // CACHELIB_HEADER


