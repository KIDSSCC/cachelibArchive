#ifndef CLIENT_API_SHM
#define CLIENT_API_SHM

#include <iostream>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "shm_util.h"


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
public:
    CachelibClient();
    void prepare_shm(string appName);
    int addpool(string poolName);
    void setKV(string key,string value);
    char* getKV(string key);
    bool delKV(string key);

    //util
    int getPid(){return this->pid;};
    int getHit;
};
#endif
