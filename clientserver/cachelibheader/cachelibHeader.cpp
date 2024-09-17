#include "cachelibHeader.h"

namespace facebook {
namespace cachelib_examples {


std::unique_ptr<Cache> gCache_;
PoolId defaultPool_;
size_t cacheSize = CACHE_SIZE + REDUNDARY_SIZE;
size_t poolSize = POOL_SIZE;

size_t defaultPoolSize = CACHE_SIZE;


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
    #if HYBRID_CACHE
    std::string cacheDir_ = folly::sformat("/SSDPath/nvmcache");
    cachelib::util::makeDir(cacheDir_);
    Cache::NvmCacheConfig nvmConfig;
    nvmConfig.navyConfig = getNvmConfig(cacheDir_);
        config.enableNvmCache(nvmConfig);
    #endif
    config.validate();   
}

void initializeCache(int cache_size, int pool_size)
{
    cacheSize = cache_size<0?cacheSize:(size_t)cache_size * GB_SIZE;
    poolSize = pool_size<0?poolSize:(size_t)pool_size * MB_SIZE;
    std::cout << "----- Cache Size: " << cacheSize / MB_SIZE << " MB " << std::endl;
    std::cout << "----- Each Pool Size: "<< poolSize / MB_SIZE << " MB " << std::endl;
    CacheConfig config;
    cacheConfigure(config);
    gCache_ = std::make_unique<Cache>(config);
    #if CREATE_DEFAULT_POOL
        std::cout<<"----- Global Pool Enabled\n";
        defaultPool_ = gCache_->addPool("default_",defaultPoolSize);
        std::cout<<"----- Global Pool Size: "<< defaultPoolSize / MB_SIZE << "MB" << std::endl;
    #endif
    std::cout<<"----- Create Cache Successfully\n";
}

void destroyCache()
{
    gCache_.reset();
    std::cout<<"----- Destroy Cache Successfully\n";
}

int addpool_(std::string poolName)
{
    #if CREATE_DEFAULT_POOL
        return 0;
    #endif
    cachelib::PoolId poolId = gCache_->getPoolId(poolName);
    if(poolId==-1){
        // std::cout<<"pool size is: "<<poolSize<<std::endl;
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

bool get_(CacheKey key, char getValue[])
{
    CacheReadHandle rh = gCache_->find(key);
    if(!rh){
        return false;
    }
    memcpy(getValue, reinterpret_cast<const char*>(rh->getMemory()), rh->getSize());
    // folly::StringPiece data{reinterpret_cast<const char*>(rh->getMemory()), rh->getSize()};
    return true;
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
    if(pid == -1)
    {
        std::cout<<"----------Error: pool "<< poolName <<" not exists\n";
    }
	PoolStats pstats = getPoolStat(pid);
	return pstats.poolSize;
}

void resizePool(std::string poolName, size_t newSize){
	PoolId pid = gCache_->getPoolId(poolName);
	PoolStats poolStat = gCache_->getPoolStats(pid);
	size_t currSize = poolStat.poolSize;
	std::cout<<"----- Change "<<poolName<<" From "<<currSize/MB_SIZE<<" MB To "<<newSize/MB_SIZE<<" MB "<<std::endl;
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

