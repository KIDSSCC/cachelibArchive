#include "clientAPI.h"

CachelibClient::CachelibClient()
{
    _shards = std::vector<CacheShard>(NUM_SHARDS);
    for (auto& shard : _shards) {
        shard.cache.reserve(_shard_capacity);
    }
}

CachelibClient::~CachelibClient()
{
    // Empty implementation
}

void CachelibClient::prepare_shm(string appName)
{
    // Empty implementation
}

int CachelibClient::addpool(string poolName)
{
    // Empty implementation
    return 0;
}

void CachelibClient::setKV(const std::string& key, const std::string& value)
{
    auto& shard = _get_shard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);

    auto& cache = shard.cache;
    auto& lru = shard.lru;
    auto it = cache.find(key);
    if (it != cache.end()) {
        lru.splice(lru.begin(), lru, it->second.second);
        it->second.first = value;
        return;
    }

    if (cache.size() == _shard_capacity) {
        auto last = lru.end(); last--;
        cache.erase(last->first);
        lru.pop_back();
    }

    lru.push_front({key, value});
    cache[key] = {lru.begin()->second, lru.begin()};
}

string CachelibClient::getKV(const std::string& key)
{
    auto& shard = _get_shard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);

    auto& cache = shard.cache;
    auto it = cache.find(key);
    if (it == cache.end()) {
        return "";
    }

    auto& lru = shard.lru;
    lru.splice(lru.begin(), lru, it->second.second);
    return it->second.first;
}

bool CachelibClient::delKV(const std::string& key) {
    auto& shard = _get_shard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);

    auto& cache = shard.cache;
    auto it = cache.find(key);
    if (it == cache.end()) {
        return false;
    }

    auto& lru = shard.lru;
    lru.erase(it->second.second);
    cache.erase(it);
    return true;
}
