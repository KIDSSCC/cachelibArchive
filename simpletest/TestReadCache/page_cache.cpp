#include "glog/logging.h"

#include "common.h"
#include "errorcode.h"
#include "page_cache.h"

namespace HybridCache {

/*
借助CAS机制进行原子上锁，0的时候代表未锁，1的时候代表上锁
*/
bool PageCache::Lock(char* pageMemory) {
   if (!cfg_.EnableCAS) return true;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   uint8_t lockExpected = 0;
   return __atomic_compare_exchange_n(lock, &lockExpected, 1, true,
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

//CAS实现解锁
void PageCache::UnLock(char* pageMemory) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}

//对页面的NewVer+1，NewVer位于pageMemory后两字节处
uint8_t PageCache::AddNewVer(char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    uint8_t* newVer = reinterpret_cast<uint8_t*>(pageMemory + 2);
    return __atomic_add_fetch(newVer, 1, __ATOMIC_SEQ_CST);
}

//对页面的LastVer设置为指定的newVer，LastVer位于pageMemory后一字节处
void PageCache::SetLastVer(char* pageMemory, uint8_t newVer) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lastVer = reinterpret_cast<uint8_t*>(pageMemory + 1);
   __atomic_store_n(lastVer, newVer, __ATOMIC_SEQ_CST);
}

//获取LastVer
uint8_t PageCache::GetLastVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* lastVer = reinterpret_cast<const uint8_t*>(pageMemory + 1);
    return __atomic_load_n(lastVer, __ATOMIC_SEQ_CST);
}

//获取NewVer
uint8_t PageCache::GetNewVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* newVer = reinterpret_cast<const uint8_t*>(pageMemory + 2);
    return __atomic_load_n(newVer, __ATOMIC_SEQ_CST);
}

//将一片空间对应的位图置位或清除
void PageCache::SetBitMap(char* pageMemory, int pos, int len, bool valid) {
    //页的起始地址+元数据地址
    char* x = pageMemory + cfg_.PageMetaSize;
    //计算出在位图中，起始字节是哪一个
    uint32_t startByte = pos / BYTE_LEN;
    // head byte
    if (pos % BYTE_LEN > 0) {
        //计算起始字节中有多少位可以写
        int headByteSetLen = BYTE_LEN - pos % BYTE_LEN;
        headByteSetLen = headByteSetLen > len ? len : headByteSetLen;
        len -= headByteSetLen;
        while (headByteSetLen) {
            if (valid)
                SetBit(x+startByte, pos%BYTE_LEN+(--headByteSetLen));
            else
                ClearBit(x+startByte, pos%BYTE_LEN+(--headByteSetLen));
        }
        ++startByte;
    }
    // mid bytes
    int midLen = len / BYTE_LEN;
    if (midLen > 0) {
        if (valid)
            memset(x+startByte, UINT8_MAX, midLen);
        else
            memset(x+startByte, 0, midLen);
        len -= BYTE_LEN * midLen;
        startByte += midLen;
    }
    // tail byte
    while (len > 0) {
        if (valid)
            SetBit(x+startByte, --len);
        else
            ClearBit(x+startByte, --len);
    }
}

//进行cachelib的配置，创建缓存与缓存池
int PageCacheImpl::Init() {
    const uint64_t REDUNDANT_SIZE = 1024 * 1024 * 1024;
    const unsigned bucketsPower = 25;
    const unsigned locksPower = 15;

    Cache::Config config;
    config
        .setCacheSize(cfg_.MaxCacheSize + REDUNDANT_SIZE)
        .setCacheName(cfg_.CacheName)
        .setAccessConfig({bucketsPower, locksPower})
        .validate();
    if (cfg_.CacheLibCfg.EnableNvmCache) {
        Cache::NvmCacheConfig nvmConfig;
        std::vector<std::string> raidPaths;
        for (int i=0; i<cfg_.CacheLibCfg.RaidFileNum; ++i) {
            raidPaths.push_back(cfg_.CacheLibCfg.RaidPath + std::to_string(i));
        }
        nvmConfig.navyConfig.setRaidFiles(raidPaths,
                cfg_.CacheLibCfg.RaidFileSize, false);
        nvmConfig.navyConfig.blockCache()
            .setDataChecksum(cfg_.CacheLibCfg.DataChecksum);
        nvmConfig.navyConfig.setReaderAndWriterThreads(1, 1, 0, 0);

        config.enableNvmCache(nvmConfig).validate();
    }
    cache_ = std::make_unique<Cache>(config);
    pool_ = cache_->addPool(cfg_.CacheName + "_pool", cfg_.MaxCacheSize);

    LOG(WARNING) << "[PageCache]Init, name:" << config.getCacheName()
                 << ", size:" << config.getCacheSize()
                 << ", dir:" << config.getCacheDir();
    return SUCCESS;
}

//销毁缓存
int PageCacheImpl::Close() {
    if (cache_)
        cache_.reset();
    LOG(WARNING) << "[PageCache]Close, name:" << cfg_.CacheName;
    return SUCCESS;
}

//向一个页面中写入数据，并更新其版本，位图等信息
//从缓冲区中，将长度为len的内容写入到页面的pagePos处，每次只处理一页
int PageCacheImpl::Write(const std::string &key,
                         uint32_t pagePos,
                         uint32_t length,
                         const char *buf) {
    //确保当前要进行的写入是只针对一个页面的写入
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    Cache::WriteHandle writeHandle = nullptr;
    char* pageValue = nullptr;
    while (true) {
        writeHandle = std::move(FindOrCreateWriteHandle(key));
        pageValue = reinterpret_cast<char*>(writeHandle->getMemory());
        if (Lock(pageValue)) break;
    }

    uint64_t realOffset = cfg_.PageMetaSize + bitmapSize_ + pagePos;
    uint8_t newVer = AddNewVer(pageValue);
    std::memcpy(pageValue + realOffset, buf, length);
    SetBitMap(pageValue, pagePos, length, true);
    SetLastVer(pageValue, newVer);
    UnLock(pageValue);
    return SUCCESS;
}

//遍历位图，寻找全部的有效数据块，将其复制到缓冲区中，并记录边界信息
//pagePos是相对该页中实际存储数据的起始

int PageCacheImpl::Read(const std::string &key,
                        uint32_t pagePos,
                        uint32_t length,
                        char *buf,
                    std::vector<std::pair<size_t, size_t>>& dataBoundary) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    while (true) {
        //获取句柄，等待数据准备完成
        auto readHandle = cache_->find(key);
        if (!readHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        while (!readHandle.isReady());

        const char* pageValue = reinterpret_cast<const char*>(
                readHandle->getMemory());
        uint8_t lastVer = GetLastVer(pageValue);
        uint8_t newVer = GetNewVer(pageValue);
        //要求新旧版本需要一致
        if (lastVer != newVer) continue;

        dataBoundary.clear();
        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        uint32_t cur = pagePos;
        //在位图部分寻找每一段连续的1，将其保存至buf缓冲区，并记录其边界信息
        while (cur < pagePos+length) {
            //在位图中定位字节，
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            // 同一批判断的长度为64位(8字节)，在某一判定条件下，快速检查8字节是否为全1或者全0，如果是全1或者全0则isBatFuncValid为true，如果全1则batByteValid为true
            // 判定条件为cur处于一个字节的起始位置，且要读取的剩余长度大于batLen
            uint16_t batLen = 0;
            bool batByteValid = false, isBatFuncValid = false;

            batLen = 64;
            if (cur % batLen == 0 && (pagePos+length-cur) >= batLen) {
                uint64_t byteValue = *reinterpret_cast<const uint64_t*>(byte);
                if (byteValue == UINT64_MAX) {
                    batByteValid = true;
                    isBatFuncValid = true;
                } else if (byteValue == 0)  {
                    isBatFuncValid = true;
                }
            }

            // 如果该段为全1或全0，且与其前一段是连续的，则该段可以直接略过
            // continuousLen为0的时候，continuousDataValid是没有意义的。
            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            //该段不为全1或全0，或该段并不完整，或该段与之前的并不连续
            //会检查在当前批中，有多少位和之前是连续的
            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                // bufOff：当前这段连续有效的部分，其相对于pagePos的偏移
                uint32_t bufOff = cur - continuousLen - pagePos;
                // pageOff:当前这段连续有效的部分，其相对于page起始的偏移
                uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                                   cur - continuousLen;
                std::memcpy(buf + bufOff, pageValue + pageOff, continuousLen);
                dataBoundary.push_back(std::make_pair(bufOff, continuousLen));
            }

            continuousDataValid = curByteValid;
            continuousLen = 1;
            ++cur;
        }
        if (continuousDataValid) {
            uint32_t bufOff = cur - continuousLen - pagePos;
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                               cur - continuousLen;
            std::memcpy(buf + bufOff, pageValue + pageOff, continuousLen);
            dataBoundary.push_back(std::make_pair(bufOff, continuousLen));
        }

        newVer = GetNewVer(pageValue);
        if (lastVer == newVer) break;
    }

    // if (SUCCESS != res) {
    //     LOG(ERROR) << "[PageCache]Read failed, name:" << cfg_.CacheName
    //                << ", key:" << key << ", start:" << pagePos
    //                << ", len:" << length << ", res:" << res;
    // }
    return res;
}

// 从一个key所对应的椤????，取出全部的数据，记录其在该页中的偏移以及长度，保存在dataSegments中
int PageCacheImpl::GetAllCache(const std::string &key,
                    std::vector<std::pair<ByteBuffer, size_t>>& dataSegments) {
    assert(cache_);
    uint32_t pageSize = cfg_.PageBodySize;

    int res = SUCCESS;
    while (true) {
        auto readHandle = cache_->find(key);
        if (!readHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        while (!readHandle.isReady());

        const char* pageValue = reinterpret_cast<const char*>(
                readHandle->getMemory());
        uint8_t lastVer = GetLastVer(pageValue);
        uint8_t newVer = GetNewVer(pageValue);
        if (lastVer != newVer) continue;

        dataSegments.clear();
        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        uint32_t cur = 0;
        while (cur < pageSize) {
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            uint16_t batLen = 0;
            bool batByteValid = false, isBatFuncValid = false;

            batLen = 64;
            if (cur % batLen == 0 && (pageSize-cur) >= batLen) {
                uint64_t byteValue = *reinterpret_cast<const uint64_t*>(byte);
                if (byteValue == UINT64_MAX) {
                    batByteValid = true;
                    isBatFuncValid = true;
                } else if (byteValue == 0)  {
                    isBatFuncValid = true;
                }
            }

            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                                   cur - continuousLen;
                //组成的pair：
                //first:一个ByteBuffer，对应有效的这段数字
                dataSegments.push_back(std::make_pair(
                    ByteBuffer(const_cast<char*>(pageValue + pageOff), continuousLen),
                    cur - continuousLen));
            }

            continuousDataValid = curByteValid;
            continuousLen = 1;
            ++cur;
        }
        if (continuousDataValid) {
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                               cur - continuousLen;
            dataSegments.push_back(std::make_pair(
                ByteBuffer(const_cast<char*>(pageValue + pageOff), continuousLen),
                cur - continuousLen));
        }

        newVer = GetNewVer(pageValue);
        if (lastVer == newVer) break;
    }

    // if (SUCCESS != res) {
    //     LOG(ERROR) << "[PageCache]Get all cache failed, name:" << cfg_.CacheName
    //                << ", key:" << key << ", res:" << res;
    // }
    return res;
}

// 根据命名应该是想要删除一页中的部分内容，同时检查�??果该页中所有内容为空，则该页作废
int PageCacheImpl::DeletePart(const std::string &key,
                              uint32_t pagePos,
                              uint32_t length) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    Cache::WriteHandle writeHandle = nullptr;
    char* pageValue = nullptr;
    while (true) {
        writeHandle = cache_->findToWrite(key);
        if (!writeHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        pageValue = reinterpret_cast<char*>(writeHandle->getMemory());
        if (Lock(pageValue)) break;
    }

    if (SUCCESS == res) {
        uint8_t newVer = AddNewVer(pageValue);
        SetBitMap(pageValue, pagePos, length, false);

        bool isEmpty = true;
        uint32_t pos = 0;
        // 遍历位图中的内容，检查从哪里开始非0
        while (pos < bitmapSize_) {
            if (*(pageValue + cfg_.PageMetaSize + pos) != 0) {
                isEmpty = false;
                break;
            }
            ++pos;
        }

        // �??这里使整个页失效了
        bool isDel = false;
        if (isEmpty) {
            if (cache_->remove(writeHandle) == Cache::RemoveRes::kSuccess) {
                pageNum_.fetch_sub(1);
                pagesList_.erase(key);
                isDel = true;
            } else {
                res = PAGE_DEL_FAIL;
            }
        }

        //如果当前页仍然存在，没有因涓????部为空而被删除，则设置其版本
        if (!isDel) {
            SetLastVer(pageValue, newVer);
            UnLock(pageValue);
        }
    }

    // if (SUCCESS != res) {
    //     LOG(ERROR) << "[PageCache]Delete part failed, name:" << cfg_.CacheName
    //                << ", key:" << key << ", start:" << pagePos
    //                << ", len:" << length << ", res:" << res;
    // }
    return res;
}

// 把某一个key对应的一整椤????除
int PageCacheImpl::Delete(const std::string &key) {
    assert(cache_);

    int res = SUCCESS;
    Cache::WriteHandle writeHandle = nullptr;
    char* pageValue = nullptr;
    while (true) {
        writeHandle = cache_->findToWrite(key);
        if (!writeHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        pageValue = reinterpret_cast<char*>(writeHandle->getMemory());
        if (Lock(pageValue)) break;
    }

    if (SUCCESS == res) {
        uint8_t newVer = AddNewVer(pageValue);
        bool isDel = false;
        if (cache_->remove(writeHandle) == Cache::RemoveRes::kSuccess) {
            pageNum_.fetch_sub(1);
            pagesList_.erase(key);
            isDel = true;
        } else {
            res = PAGE_DEL_FAIL;
        }

        if (!isDel) {
            SetLastVer(pageValue, newVer);
            UnLock(pageValue);
        }
    }

    // if (SUCCESS != res) {
    //     LOG(ERROR) << "[PageCache]Delete failed, name:" << cfg_.CacheName
    //                << ", key:" << key << ", res:" << res;
    // }
    return res;
}

//查找或申请一个页面的writehandle
Cache::WriteHandle PageCacheImpl::FindOrCreateWriteHandle(const std::string &key) {
    auto writeHandle = cache_->findToWrite(key);
    if (!writeHandle) {
        //若并不存在与当前key对应的页面锛????导致writeHandle为空，进入该路径
        writeHandle = cache_->allocate(pool_, key, GetRealPageSize());
        assert(writeHandle);
        assert(writeHandle->getMemory());
        // need init
        //初始化过程中并没有对实际存储数据的部分清零
        memset(writeHandle->getMemory(), 0, cfg_.PageMetaSize + bitmapSize_);

        //进行页的计数并将申请的页所对应的key鎻????到跳跃表中
        if (cfg_.CacheLibCfg.EnableNvmCache) {
            // insertOrReplace will insert or replace existing item for the key,
            // and return the handle of the replaced old item
            // Note: write cache nonsupport NVM, because it will be replaced
            if (!cache_->insertOrReplace(writeHandle)) {
                pageNum_.fetch_add(1);
                pagesList_.insert(key);
            }
        } else {
            if (cache_->insert(writeHandle)) {
                //insert操作在当前KV已经存在时会失败，应该调用的是insertorreplace
                pageNum_.fetch_add(1);
                pagesList_.insert(key);
            } else {
                writeHandle = cache_->findToWrite(key);
            }
        }
    }
    return writeHandle;
}

}  // namespace HybridCache

