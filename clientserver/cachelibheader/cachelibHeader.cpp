#include "cachelibHeader.h"

namespace facebook {
namespace cachelib_examples {


std::unique_ptr<Cache> gCache_;
PoolId defaultPool_;
size_t cacheSize = CACHE_SIZE + REDUNDARY_SIZE;
size_t poolSize = POOL_SIZE;
bool create_default_pool = false;

/*
getNvmConfig：生成Memory+SSD混合缓存中，SSD缓存的配置
params：
    cacheDir：SSD文件路径位置
returns：
    NavyConfig：SSD混合配置
*/
NavyConfig getNvmConfig(const std::string& cacheDir){
	NavyConfig config{};
	config.setSimpleFile(cacheDir + "/navy", 4 * GB_SIZE);
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

/*
cacheConfigure：对传入的CacheConfig进行配置
params：
    config：待设置的config
returns：
    void
*/
void cacheConfigure(CacheConfig& config)
{
    config.setCacheSize(cacheSize);  
    config.setCacheName("Cachelib Cache");
    config.setAccessConfig({25, 10});   //bucket power = 25,lock power = 10

    // 动态缓存大小调整
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

/*
initializeCache：按照指定的缓存大小创建缓存实例。传入参数的优先级高于配置文件
params：
    cache_size：缓存实例大小，不包含冗余大小，单位为MB
    pool_size：缓存池大小，确定后续创建新的缓存池时的大小，单位为MB
    default_pool：是否创建全局唯一缓存池
returns：
    void
*/
void initializeCache(int cache_size, int pool_size, int default_pool)
{
    cacheSize = cache_size < 0 ? cacheSize:((size_t)cache_size * MB_SIZE + REDUNDARY_SIZE);
    poolSize = pool_size < 0 ? poolSize:(size_t)pool_size * MB_SIZE;
    create_default_pool = default_pool || CREATE_DEFAULT_POOL;
    XLOG(INFO) << "---------- CacheLib Info ----------";
    XLOG(INFO) << "Cache Size is: " << cacheSize / MB_SIZE << " MB";
    XLOG(INFO) << "Each Pool Size is: " << poolSize / MB_SIZE << " MB";
    
    CacheConfig config;
    cacheConfigure(config);
    gCache_ = std::make_unique<Cache>(config);
    if(create_default_pool)
    {
        XLOG(INFO) << "Global Pool Enabled";
        defaultPool_ = gCache_->addPool("default_",(size_t)cache_size * MB_SIZE);
        XLOG(INFO) << "Global Pool Size is: " << cache_size << " MB";
    }
    XLOG(INFO) << "----------Info End ----------";
    XLOG(INFO) << "Create Cache Successfully";
}

/*
destroyCache：回收缓存实例
*/
void destroyCache()
{
    gCache_.reset();
    XLOG(INFO) << "Destroy Cache Successfully";
}

/*
addpool_：根据缓存池名创建新的缓存池，获取缓存池编号。在已有全局唯一缓存池情况下，返回全局池编号
params：
    poolName：新注册缓存池的标识符
returns：
    int：缓存池编号
*/
int addpool_(std::string poolName)
{
    // 已注册全局池，返回全局池编号
    if(create_default_pool)
        return 0;

    // 检查缓存池是否已存在，不存在则创建新的缓存池
    cachelib::PoolId poolId = gCache_->getPoolId(poolName);
    if(poolId==-1){
        poolId = gCache_->addPool(poolName, poolSize);
    }
    return poolId;
}

/*
set_：向缓存池中存入新的Key-Value
params：
    pid：要存入的缓存池编号
    key：字符串类型的key
    value：待存入的value
returns：
    bool：set操作的结果
*/
bool set_(cachelib::PoolId pid, CacheKey key, const char* value)
{
    CacheWriteHandle wh = gCache_->allocate(pid, key, std::strlen(value));
    if(!wh)
        return false;
    std::memcpy(wh->getMemory(), value, std::strlen(value));
    gCache_->insertOrReplace(wh);
    return true;
}

/*
get_：在缓存实例中查询某一key所对应的value
params：
    key：字符串类型的待查询key
    getValue：保存查询结果的指针
returns：
    bool：查询操作的结果
*/
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

/*
del_：在缓存实例中移除Key-Value
params：
    key：字符串类型的待删除key
returns：
    bool：删除操作的结果
*/
bool del_(CacheKey key)
{
    RemoveRes rr = gCache_->remove(key);
    if(rr == RemoveRes::kSuccess)
        return true;
    else
        return false;
}

/*
getPoolIds_：获取当前缓存实例中所有缓存池编号
returns：
    std::set<PoolId>：保存所有缓存池编号的列表
*/
std::set<PoolId> getPoolIds_(){
	return gCache_->getPoolIds();
}

/*
getAvailableSize：获取缓存实例当前未分配给缓存池的空间
returns：
    size_t：未分配的空间大小，单位为字节
*/
size_t getAvailableSize(){
	CacheMemoryStats currStats = gCache_->getCacheMemoryStats();
	return currStats.unReservedSize;	
}

/*
getPoolStat：根据缓存池编号获取缓存池状态信息
params：
    pid：缓存池编号
returns：
    PoolStats：缓存池状态参数
*/
PoolStats getPoolStat(PoolId pid){
	return gCache_->getPoolStats(pid);
}

/*
getPoolSizeFromName：根据缓存池标识符查询缓存池大小
params：
    poolName：缓存池标识符
returns：
    size_t：缓存池大小，单位为字节
*/
size_t getPoolSizeFromName(std::string poolName){
	PoolId pid = gCache_->getPoolId(poolName);
    if(pid == -1)
    {
        XLOG(ERR) << "Error: pool "<< poolName <<" not exists";
    }
	PoolStats pstats = getPoolStat(pid);
	return pstats.poolSize;
}

/*
resizePool：缓存池大小动态调整
params：
    poolName：缓存池标识符
    newSize：缓存池新的大小，单位为字节
returns：
    void
*/
void resizePool(std::string poolName, size_t newSize){
	PoolId pid = gCache_->getPoolId(poolName);
	PoolStats poolStat = gCache_->getPoolStats(pid);
	size_t currSize = poolStat.poolSize;
    XLOG(ERR) <<"Change poolName: "<< poolName <<" From "<< currSize/MB_SIZE <<" MB To "<< newSize/MB_SIZE <<" MB ";
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

