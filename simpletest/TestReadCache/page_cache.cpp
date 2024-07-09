#include "glog/logging.h"

#include "common.h"
#include "errorcode.h"
#include "page_cache.h"

namespace HybridCache {

/*
����CAS���ƽ���ԭ��������0��ʱ�����δ����1��ʱ���������
*/
bool PageCache::Lock(char* pageMemory) {
   if (!cfg_.EnableCAS) return true;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   uint8_t lockExpected = 0;
   return __atomic_compare_exchange_n(lock, &lockExpected, 1, true,
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

//CASʵ�ֽ���
void PageCache::UnLock(char* pageMemory) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory);
   __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}

//��ҳ���NewVer+1��NewVerλ��pageMemory�����ֽڴ�
uint8_t PageCache::AddNewVer(char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    uint8_t* newVer = reinterpret_cast<uint8_t*>(pageMemory + 2);
    return __atomic_add_fetch(newVer, 1, __ATOMIC_SEQ_CST);
}

//��ҳ���LastVer����Ϊָ����newVer��LastVerλ��pageMemory��һ�ֽڴ�
void PageCache::SetLastVer(char* pageMemory, uint8_t newVer) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lastVer = reinterpret_cast<uint8_t*>(pageMemory + 1);
   __atomic_store_n(lastVer, newVer, __ATOMIC_SEQ_CST);
}

//��ȡLastVer
uint8_t PageCache::GetLastVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* lastVer = reinterpret_cast<const uint8_t*>(pageMemory + 1);
    return __atomic_load_n(lastVer, __ATOMIC_SEQ_CST);
}

//��ȡNewVer
uint8_t PageCache::GetNewVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* newVer = reinterpret_cast<const uint8_t*>(pageMemory + 2);
    return __atomic_load_n(newVer, __ATOMIC_SEQ_CST);
}

//��һƬ�ռ��Ӧ��λͼ��λ�����
void PageCache::SetBitMap(char* pageMemory, int pos, int len, bool valid) {
    //ҳ����ʼ��ַ+Ԫ���ݵ�ַ
    char* x = pageMemory + cfg_.PageMetaSize;
    //�������λͼ�У���ʼ�ֽ�����һ��
    uint32_t startByte = pos / BYTE_LEN;
    // head byte
    if (pos % BYTE_LEN > 0) {
        //������ʼ�ֽ����ж���λ����д
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

//����cachelib�����ã����������뻺���
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

//���ٻ���
int PageCacheImpl::Close() {
    if (cache_)
        cache_.reset();
    LOG(WARNING) << "[PageCache]Close, name:" << cfg_.CacheName;
    return SUCCESS;
}

//��һ��ҳ����д�����ݣ���������汾��λͼ����Ϣ
//�ӻ������У�������Ϊlen������д�뵽ҳ���pagePos����ÿ��ֻ����һҳ
int PageCacheImpl::Write(const std::string &key,
                         uint32_t pagePos,
                         uint32_t length,
                         const char *buf) {
    //ȷ����ǰҪ���е�д����ֻ���һ��ҳ���д��
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

//����λͼ��Ѱ��ȫ������Ч���ݿ飬���临�Ƶ��������У�����¼�߽���Ϣ
//pagePos����Ը�ҳ��ʵ�ʴ洢���ݵ���ʼ

int PageCacheImpl::Read(const std::string &key,
                        uint32_t pagePos,
                        uint32_t length,
                        char *buf,
                    std::vector<std::pair<size_t, size_t>>& dataBoundary) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    while (true) {
        //��ȡ������ȴ�����׼�����
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
        //Ҫ���¾ɰ汾��Ҫһ��
        if (lastVer != newVer) continue;

        dataBoundary.clear();
        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        uint32_t cur = pagePos;
        //��λͼ����Ѱ��ÿһ��������1�����䱣����buf������������¼��߽���Ϣ
        while (cur < pagePos+length) {
            //��λͼ�ж�λ�ֽڣ�
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            // ͬһ���жϵĳ���Ϊ64λ(8�ֽ�)����ĳһ�ж������£����ټ��8�ֽ��Ƿ�Ϊȫ1����ȫ0�������ȫ1����ȫ0��isBatFuncValidΪtrue�����ȫ1��batByteValidΪtrue
            // �ж�����Ϊcur����һ���ֽڵ���ʼλ�ã���Ҫ��ȡ��ʣ�೤�ȴ���batLen
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

            // ����ö�Ϊȫ1��ȫ0��������ǰһ���������ģ���öο���ֱ���Թ�
            // continuousLenΪ0��ʱ��continuousDataValid��û������ġ�
            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            //�öβ�Ϊȫ1��ȫ0����öβ�����������ö���֮ǰ�Ĳ�������
            //�����ڵ�ǰ���У��ж���λ��֮ǰ��������
            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                // bufOff����ǰ���������Ч�Ĳ��֣��������pagePos��ƫ��
                uint32_t bufOff = cur - continuousLen - pagePos;
                // pageOff:��ǰ���������Ч�Ĳ��֣��������page��ʼ��ƫ��
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

// ��һ��key����Ӧ���????��ȡ��ȫ�������ݣ���¼���ڸ�ҳ�е�ƫ���Լ����ȣ�������dataSegments��
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
                //��ɵ�pair��
                //first:һ��ByteBuffer����Ӧ��Ч���������
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

// ��������Ӧ������Ҫɾ��һҳ�еĲ������ݣ�ͬʱ����??����ҳ����������Ϊ�գ����ҳ����
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
        // ����λͼ�е����ݣ��������￪ʼ��0
        while (pos < bitmapSize_) {
            if (*(pageValue + cfg_.PageMetaSize + pos) != 0) {
                isEmpty = false;
                break;
            }
            ++pos;
        }

        // �??����ʹ����ҳʧЧ��
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

        //�����ǰҳ��Ȼ���ڣ�û�����????��Ϊ�ն���ɾ������������汾
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

// ��ĳһ��key��Ӧ��һ���????��
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

//���һ�����һ��ҳ���writehandle
Cache::WriteHandle PageCacheImpl::FindOrCreateWriteHandle(const std::string &key) {
    auto writeHandle = cache_->findToWrite(key);
    if (!writeHandle) {
        //�����������뵱ǰkey��Ӧ��ҳ���????����writeHandleΪ�գ������·��
        writeHandle = cache_->allocate(pool_, key, GetRealPageSize());
        assert(writeHandle);
        assert(writeHandle->getMemory());
        // need init
        //��ʼ�������в�û�ж�ʵ�ʴ洢���ݵĲ�������
        memset(writeHandle->getMemory(), 0, cfg_.PageMetaSize + bitmapSize_);

        //����ҳ�ļ������������ҳ����Ӧ��key�????����Ծ����
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
                //insert�����ڵ�ǰKV�Ѿ�����ʱ��ʧ�ܣ�Ӧ�õ��õ���insertorreplace
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

