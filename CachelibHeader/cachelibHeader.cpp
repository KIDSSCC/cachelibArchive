#include "cachelibHeader.h"

namespace facebook {
namespace cachelib_examples {


std::unique_ptr<Cache> gCache_;
PoolId defaultPool_;
size_t cacheSize = (size_t)16 * 1024 * 1024 * 1024 + (size_t)4 * 1024 * 1024;
size_t poolSize = (size_t)16 * 1024 * 1024 * 1024 + (size_t)0 * 1024 * 1024;

size_t defaultPoolSize = (size_t)120 * 1024 * 1024 + (size_t)0 * 1024 * 1024;


NavyConfig getNvmConfig(const std::string& cacheDir){
	NavyConfig config{};
	config.setSimpleFile(cacheDir + "/navy", 6 * 1024ULL * 1024ULL * 1024ULL);
	config.setBlockSize(4096);
	config.setDeviceMetadataSize(4 * 1024 * 1024);
	config.setNavyReqOrderingShards(10);
	config.blockCache().setRegionSize(4 * 1024 * 1024);
	config.bigHash()
		.setSizePctAndMaxItemSize(50, 100)
		.setBucketSize(4096)
		.setBucketBfSize(8);
	return config;
}

void cacheConfigure(CacheConfig& config)
{
    config.setCacheSize(cacheSize);  
    config.setCacheName("Cachelib Cache");
    config.setAccessConfig({25, 10});   //bucket power = 25,lock power = 10

    const uint32_t poolResizeSlabsPerIter = 100000;
    config.enablePoolResizing(std::make_shared<RebalanceStrategy>(),std::chrono::milliseconds{1}, poolResizeSlabsPerIter);

    //nvm config
    std::string cacheDir_ = folly::sformat("/SSDPath/nvmcache");
    cachelib::util::makeDir(cacheDir_);
    Cache::NvmCacheConfig nvmConfig;
    nvmConfig.navyConfig = getNvmConfig(cacheDir_);
    //config.enableNvmCache(nvmConfig);
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
    if(!rh){
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
PoolStats getPoolStat(PoolId pid){
	return gCache_->getPoolStats(pid);
}

size_t getPoolSizeFromName(std::string poolName){
	PoolId pid = gCache_->getPoolId(poolName);
	PoolStats pstats = getPoolStat(pid);
	return pstats.poolSize;
}

void resizePool(std::string poolName, size_t newSize){
	PoolId pid = gCache_->getPoolId(poolName);
	PoolStats poolStat = gCache_->getPoolStats(pid);
	size_t currSize = poolStat.poolSize;
	std::cout<<"change "<<poolName<<" from "<<currSize<<" to "<<newSize<<std::endl;
	if(newSize>currSize){
		//growPool
		gCache_->growPool(pid, newSize-currSize);
	}else if(newSize<currSize){
		//shrinkPool
		gCache_->shrinkPool(pid, currSize-newSize);
	}
}

}
}

