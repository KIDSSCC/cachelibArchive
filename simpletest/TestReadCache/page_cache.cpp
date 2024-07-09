#include "glog/logging.h"

#include "common.h"
#include "errorcode.h"
#include "page_cache.h"

namespace HybridCache {

/*
½èÖúCAS»úÖÆ½øĞĞÔ­×ÓÉÏËø£¬0µÄÊ±ºò´ú±íÎ´Ëø£¬1µÄÊ±ºò´ú±íÉÏËø
*/
bool PageCache::Lock(char* pageMemory) {
   if (!cfg_.EnableCAS) return true;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   uint8_t lockExpected = 0;
   return __atomic_compare_exchange_n(lock, &lockExpected, 1, true,
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

//CASÊµÏÖ½âËø
void PageCache::UnLock(char* pageMemory) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}

//¶ÔÒ³ÃæµÄNewVer+1£¬NewVerÎ»ÓÚpageMemoryºóÁ½×Ö½Ú´¦
uint8_t PageCache::AddNewVer(char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    uint8_t* newVer = reinterpret_cast<uint8_t*>(pageMemory + 2);
    return __atomic_add_fetch(newVer, 1, __ATOMIC_SEQ_CST);
}

//¶ÔÒ³ÃæµÄLastVerÉèÖÃÎªÖ¸¶¨µÄnewVer£¬LastVerÎ»ÓÚpageMemoryºóÒ»×Ö½Ú´¦
void PageCache::SetLastVer(char* pageMemory, uint8_t newVer) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lastVer = reinterpret_cast<uint8_t*>(pageMemory + 1);
   __atomic_store_n(lastVer, newVer, __ATOMIC_SEQ_CST);
}

//»ñÈ¡LastVer
uint8_t PageCache::GetLastVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* lastVer = reinterpret_cast<const uint8_t*>(pageMemory + 1);
    return __atomic_load_n(lastVer, __ATOMIC_SEQ_CST);
}

//»ñÈ¡NewVer
uint8_t PageCache::GetNewVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* newVer = reinterpret_cast<const uint8_t*>(pageMemory + 2);
    return __atomic_load_n(newVer, __ATOMIC_SEQ_CST);
}

//½«Ò»Æ¬¿Õ¼ä¶ÔÓ¦µÄÎ»Í¼ÖÃÎ»»òÇå³ı
void PageCache::SetBitMap(char* pageMemory, int pos, int len, bool valid) {
    //Ò³µÄÆğÊ¼µØÖ·+ÔªÊı¾İµØÖ·
    char* x = pageMemory + cfg_.PageMetaSize;
    //¼ÆËã³öÔÚÎ»Í¼ÖĞ£¬ÆğÊ¼×Ö½ÚÊÇÄÄÒ»¸ö
    uint32_t startByte = pos / BYTE_LEN;
    // head byte
    if (pos % BYTE_LEN > 0) {
        //¼ÆËãÆğÊ¼×Ö½ÚÖĞÓĞ¶àÉÙÎ»¿ÉÒÔĞ´
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

//½øĞĞcachelibµÄÅäÖÃ£¬´´½¨»º´æÓë»º´æ³Ø
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

//Ïú»Ù»º´æ
int PageCacheImpl::Close() {
    if (cache_)
        cache_.reset();
    LOG(WARNING) << "[PageCache]Close, name:" << cfg_.CacheName;
    return SUCCESS;
}

//ÏòÒ»¸öÒ³ÃæÖĞĞ´ÈëÊı¾İ£¬²¢¸üĞÂÆä°æ±¾£¬Î»Í¼µÈĞÅÏ¢
//´Ó»º³åÇøÖĞ£¬½«³¤¶ÈÎªlenµÄÄÚÈİĞ´Èëµ½Ò³ÃæµÄpagePos´¦£¬Ã¿´ÎÖ»´¦ÀíÒ»Ò³
int PageCacheImpl::Write(const std::string &key,
                         uint32_t pagePos,
                         uint32_t length,
                         const char *buf) {
    //È·±£µ±Ç°Òª½øĞĞµÄĞ´ÈëÊÇÖ»Õë¶ÔÒ»¸öÒ³ÃæµÄĞ´Èë
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

//±éÀúÎ»Í¼£¬Ñ°ÕÒÈ«²¿µÄÓĞĞ§Êı¾İ¿é£¬½«Æä¸´ÖÆµ½»º³åÇøÖĞ£¬²¢¼ÇÂ¼±ß½çĞÅÏ¢
//pagePosÊÇÏà¶Ô¸ÃÒ³ÖĞÊµ¼Ê´æ´¢Êı¾İµÄÆğÊ¼

int PageCacheImpl::Read(const std::string &key,
                        uint32_t pagePos,
                        uint32_t length,
                        char *buf,
                    std::vector<std::pair<size_t, size_t>>& dataBoundary) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    while (true) {
        //»ñÈ¡¾ä±ú£¬µÈ´ıÊı¾İ×¼±¸Íê³É
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
        //ÒªÇóĞÂ¾É°æ±¾ĞèÒªÒ»ÖÂ
        if (lastVer != newVer) continue;

        dataBoundary.clear();
        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        uint32_t cur = pagePos;
        //ÔÚÎ»Í¼²¿·ÖÑ°ÕÒÃ¿Ò»¶ÎÁ¬ĞøµÄ1£¬½«Æä±£´æÖÁbuf»º³åÇø£¬²¢¼ÇÂ¼Æä±ß½çĞÅÏ¢
        while (cur < pagePos+length) {
            //ÔÚÎ»Í¼ÖĞ¶¨Î»×Ö½Ú£¬
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            // Í¬Ò»ÅúÅĞ¶ÏµÄ³¤¶ÈÎª64Î»(8×Ö½Ú)£¬ÔÚÄ³Ò»ÅĞ¶¨Ìõ¼şÏÂ£¬¿ìËÙ¼ì²é8×Ö½ÚÊÇ·ñÎªÈ«1»òÕßÈ«0£¬Èç¹ûÊÇÈ«1»òÕßÈ«0ÔòisBatFuncValidÎªtrue£¬Èç¹ûÈ«1ÔòbatByteValidÎªtrue
            // ÅĞ¶¨Ìõ¼şÎªcur´¦ÓÚÒ»¸ö×Ö½ÚµÄÆğÊ¼Î»ÖÃ£¬ÇÒÒª¶ÁÈ¡µÄÊ£Óà³¤¶È´óÓÚbatLen
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

            // Èç¹û¸Ã¶ÎÎªÈ«1»òÈ«0£¬ÇÒÓëÆäÇ°Ò»¶ÎÊÇÁ¬ĞøµÄ£¬Ôò¸Ã¶Î¿ÉÒÔÖ±½ÓÂÔ¹ı
            // continuousLenÎª0µÄÊ±ºò£¬continuousDataValidÊÇÃ»ÓĞÒâÒåµÄ¡£
            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            //¸Ã¶Î²»ÎªÈ«1»òÈ«0£¬»ò¸Ã¶Î²¢²»ÍêÕû£¬»ò¸Ã¶ÎÓëÖ®Ç°µÄ²¢²»Á¬Ğø
            //»á¼ì²éÔÚµ±Ç°ÅúÖĞ£¬ÓĞ¶àÉÙÎ»ºÍÖ®Ç°ÊÇÁ¬ĞøµÄ
            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                // bufOff£ºµ±Ç°Õâ¶ÎÁ¬ĞøÓĞĞ§µÄ²¿·Ö£¬ÆäÏà¶ÔÓÚpagePosµÄÆ«ÒÆ
                uint32_t bufOff = cur - continuousLen - pagePos;
                // pageOff:µ±Ç°Õâ¶ÎÁ¬ĞøÓĞĞ§µÄ²¿·Ö£¬ÆäÏà¶ÔÓÚpageÆğÊ¼µÄÆ«ÒÆ
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

// ´ÓÒ»¸ökeyËù¶ÔÓ¦µÄé¡????£¬È¡³öÈ«²¿µÄÊı¾İ£¬¼ÇÂ¼ÆäÔÚ¸ÃÒ³ÖĞµÄÆ«ÒÆÒÔ¼°³¤¶È£¬±£´æÔÚdataSegmentsÖĞ
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
                //×é³ÉµÄpair£º
                //first:Ò»¸öByteBuffer£¬¶ÔÓ¦ÓĞĞ§µÄÕâ¶ÎÊı×Ö
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

// ¸ù¾İÃüÃûÓ¦¸ÃÊÇÏëÒªÉ¾³ıÒ»Ò³ÖĞµÄ²¿·ÖÄÚÈİ£¬Í¬Ê±¼ì²éå??¹û¸ÃÒ³ÖĞËùÓĞÄÚÈİÎª¿Õ£¬Ôò¸ÃÒ³×÷·Ï
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
        // ±éÀúÎ»Í¼ÖĞµÄÄÚÈİ£¬¼ì²é´ÓÄÄÀï¿ªÊ¼·Ç0
        while (pos < bitmapSize_) {
            if (*(pageValue + cfg_.PageMetaSize + pos) != 0) {
                isEmpty = false;
                break;
            }
            ++pos;
        }

        // ä??ÕâÀïÊ¹Õû¸öÒ³Ê§Ğ§ÁË
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

        //Èç¹ûµ±Ç°Ò³ÈÔÈ»´æÔÚ£¬Ã»ÓĞÒòä¸????²¿Îª¿Õ¶ø±»É¾³ı£¬ÔòÉèÖÃÆä°æ±¾
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

// °ÑÄ³Ò»¸ökey¶ÔÓ¦µÄÒ»Õûé¡????³ı
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

//²éÕÒ»òÉêÇëÒ»¸öÒ³ÃæµÄwritehandle
Cache::WriteHandle PageCacheImpl::FindOrCreateWriteHandle(const std::string &key) {
    auto writeHandle = cache_->findToWrite(key);
    if (!writeHandle) {
        //Èô²¢²»´æÔÚÓëµ±Ç°key¶ÔÓ¦µÄÒ³Ãæï¼????µ¼ÖÂwriteHandleÎª¿Õ£¬½øÈë¸ÃÂ·¾¶
        writeHandle = cache_->allocate(pool_, key, GetRealPageSize());
        assert(writeHandle);
        assert(writeHandle->getMemory());
        // need init
        //³õÊ¼»¯¹ı³ÌÖĞ²¢Ã»ÓĞ¶ÔÊµ¼Ê´æ´¢Êı¾İµÄ²¿·ÖÇåÁã
        memset(writeHandle->getMemory(), 0, cfg_.PageMetaSize + bitmapSize_);

        //½øĞĞÒ³µÄ¼ÆÊı²¢½«ÉêÇëµÄÒ³Ëù¶ÔÓ¦µÄkeyæ????µ½ÌøÔ¾±íÖĞ
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
                //insert²Ù×÷ÔÚµ±Ç°KVÒÑ¾­´æÔÚÊ±»áÊ§°Ü£¬Ó¦¸Ãµ÷ÓÃµÄÊÇinsertorreplace
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

