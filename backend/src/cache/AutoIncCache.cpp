#include "AutoIncCache.h"

void AutoIncCache::initialize(CachelibClient &client)
{
    this->client = &client;
}

void AutoIncCache::destroy()
{
    // do nothing
}

void AutoIncCache::set_subpool(std::string subpool)
{
    subpool_name = subpool;
}

bool AutoIncCache::set_(const std::string& key, const std::string& value)
{
    // this chk can be removed for production
    // the maximum size of the value is 4MB
    if (value.size() > 4 * 1024 * 1024) {
        return false;
    }
    if (client) {
        std::string full_key = std::to_string(counter) + "_" + subpool_name + "_" + key;
        client->setKV(full_key, value);
        return true;
    }
    return false;
}

std::string AutoIncCache::get_(const std::string& key)
{
    if (client) {
        return client->getKV(std::to_string(counter) + "_" + subpool_name + "_" + key);
    }
    return "";
}

bool AutoIncCache::del_(const std::string& key)
{
    if (client) {
        std::string full_key = std::to_string(counter) + "_" + subpool_name + "_" + key;
        return client->delKV(full_key);
    }
    return false;
}

void AutoIncCache::invalidate_all_()
{
    counter++;
}
