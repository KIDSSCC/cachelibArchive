#ifndef CLIENT_API_SHM
#define CLIENT_API_SHM

#include <iostream>
#include <thread>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>
#include <list>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <shared_mutex>

#include "shm_util.h"

#define MAX_CACHE_ENTRIES 100000
#define NUM_SHARDS 64

using namespace std;
class CachelibClient
{
private:
    int pid;
    int msgid;
    string prefix;

    int shm_fd;
    void* shared_memory;
    sem_t* semaphore;
    sem_t* semaphore_Server;
    sem_t* semaphore_GetBack;

    //用于创建缓存池操作同步机制的信号量
    void* addpool_shared_memory;
    sem_t* sema_client;
    sem_t* sema_write_right;
    sem_t* sema_add_pool_back;
    //接收get操作返回结果
    char getValue[1024];

    struct CacheShard {
        std::unordered_map<std::string, std::pair<std::string, std::list<std::pair<std::string, std::string>>::iterator>> cache;
        std::list<std::pair<std::string, std::string>> lru;
        std::mutex mutex;
    };

    std::vector<CacheShard> _shards;
    size_t _shard_capacity = MAX_CACHE_ENTRIES / NUM_SHARDS;

public:
    CachelibClient();
    ~CachelibClient();
    void prepare_shm(string appName);
    int addpool(string poolName);
    void setKV(const std::string& key, const std::string& value);
    string getKV(const std::string& key);
    bool delKV(const std::string& key);

    CacheShard& _get_shard(const std::string& key) {
        std::hash<std::string> hasher;
        return _shards[hasher(key) % _shards.size()];
    }

    //util
    int getPid(){return this->pid;};
    int getHit;
};
#endif
