#include "backend.h"

void Backend::enable_cache(CachelibClient &client) {
    if (cache_enabled) {
        throw std::runtime_error("Cache already enabled");
    }
    cache_enabled = true;
    cache.initialize(client);
    subpool_name = "thread_" + std::to_string(thread_id);
    cache.set_subpool(subpool_name);
    debug(
        std::cout << "subpool name: " << subpool_name << std::endl;
        std::cout << "Cache enabled for thread " << thread_id << std::endl;
    );
}