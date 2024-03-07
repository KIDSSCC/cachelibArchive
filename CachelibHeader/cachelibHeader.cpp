#include "cachelibHeader.h"

namespace facebook {
namespace cachelib_examples {


std::unique_ptr<Cache> gCache_;
PoolId defaultPool_;
size_t cacheSize = (size_t)4 * 1024 * 1024 * 1024 + (size_t)4 * 1024 * 1024;
size_t poolSize = (size_t)1 * 256 * 1024 * 1024;

size_t defaultPoolSize = (size_t)1 * 1024 * 1024 * 1024;



void cacheConfigure(CacheConfig& config)
{
    config.setCacheSize(cacheSize);  //2GB cache
    config.setCacheName("Cachelib Cache");
    config.setAccessConfig({25, 10});   //bucket power = 25,lock power = 10
    config.validate();   
}

void initializeCache()
{
    CacheConfig config;
    cacheConfigure(config);
    gCache_ = std::make_unique<Cache>(config);
    std::cout<<"Create Cache successfully\n";
    //defaultPool_ = gCache_->addPool("default_",defaultPoolSize);
    //std::cout<<"defaultPool_ is: "<<(int)defaultPool_<<std::endl;
}

void destroyCache()
{
    gCache_.reset();
    std::cout<<"destroy Cache successfully\n";
}

int addpool_(std::string poolName)
{
    //return 0;
    cachelib::PoolId poolId = gCache_->getPoolId(poolName);
    if(poolId==-1){
        poolId = gCache_->addPool(poolName, poolSize);
    	poolSize = poolSize+(size_t)512 * 1024 * 1024;
    }
    return poolId;
}

bool set_(cachelib::PoolId pid, CacheKey key, const std::string& value)
{
    CacheWriteHandle wh = gCache_->allocate(pid, key, value.size());
    if(!wh)
        return false;
    std::memcpy(wh->getMemory(), value.data(), value.size());
    gCache_->insertOrReplace(wh);
    return true;
}

std::string get_(CacheKey key)
{
    CacheReadHandle rh = gCache_->find(key);
    if(!rh)
    {
        return "";
    }
    folly::StringPiece data{reinterpret_cast<const char*>(rh->getMemory()), rh->getSize()};
    return data.toString();
}

bool del_(CacheKey key)
{
    RemoveRes rr = gCache_->remove(key);
    if(rr == RemoveRes::kSuccess)
        return true;
    else
        return false;
}

std::set<PoolId> getPoolIds_(){
	return gCache_->getPoolIds();
}
size_t getAvailableSize(){
	CacheMemoryStats currStats = gCache_->getCacheMemoryStats();
	return currStats.unReservedSize;	
}

}
}
