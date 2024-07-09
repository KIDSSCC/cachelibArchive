#ifndef HYBRIDCACHE_CONFIG_H_
#define HYBRIDCACHE_CONFIG_H_

#include <map>
#include <string>

namespace HybridCache {

struct CacheLibConfig {
    bool            EnableNvmCache  = false;
    std::string     RaidPath;
    uint64_t        RaidFileNum;
    size_t          RaidFileSize;
    bool            DataChecksum    = false;
};

struct CacheConfig {
    std::string     CacheName;
    size_t          MaxCacheSize;
    uint32_t        PageBodySize;
    uint32_t        PageMetaSize;
    bool            EnableCAS;
    CacheLibConfig  CacheLibCfg;
};

struct ReadCacheConfig {
    CacheConfig     CacheCfg;
    uint64_t        DownloadNormalFlowLimit;
    uint64_t        DownloadBurstFlowLimit;
};

struct WriteCacheConfig {
    CacheConfig     CacheCfg;
    uint32_t        CacheSafeRatio;  // cache safety concern threshold (percent)
};

struct HybridCacheConfig {
    ReadCacheConfig ReadCacheCfg;
    WriteCacheConfig WriteCacheCfg;
    uint32_t        ThreadNum;
    uint32_t        BackFlushCacheRatio;
    uint64_t        UploadNormalFlowLimit;
    uint64_t        UploadBurstFlowLimit;
    std::string     LogPath;
    uint32_t        LogLevel;
    bool            EnableLog = true;
    bool            UseGlobalCache = false;
    bool            FlushToRead = false;  // write to read cache after flush
    bool            CleanCacheByOpen = false;  // clean read cache when open file
};

bool GetHybridCacheConfig(const std::string& file, HybridCacheConfig& cfg);
bool CheckConfig(const HybridCacheConfig& cfg);

class Configuration {
 public:
    bool LoadConfig(const std::string& file);
    void PrintConfig();

    /*
    * @brief GetValueFatalIfFail Get the value of the specified config item
    * log it if get error
    *
    * @param[in] key config name
    * @param[out] value config value
    *
    * @return
    */
    template <class T>
    void GetValueFatalIfFail(const std::string& key, T& value);

 private:
    std::string confFile_;
    std::map<std::string, std::string>  config_;
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_CONFIG_H_

