#ifndef CLIENT_API_SHM
#define CLIENT_API_SHM

#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include "shm_util.h"

#define HIT_CONTROL 1


using namespace std;
class CachelibClient
{
private:
	// 缓存池编号
    int pid;
	// 区分Key的唯一前缀
    string prefix;
	// 共享内存信道文件描述符
    int shm_fd;
	// 共享内存指针
    void* shared_memory;
	// 信号量标识符
	string shmId;

    sem_t* semaphore;
    sem_t* semaphore_Server;
    sem_t* semaphore_GetBack;
public:
    CachelibClient();
    ~CachelibClient();
    void prepare_shm(string appName);
    int addpool(string poolName);
    void haspool();
    void setKV(string& key,const string& value);
    string getKV(const string& key);
    bool delKV(string& key);

    //util
    int getPid(){return this->pid;};

    //random
    random_device rd;
    mt19937 gen;
    uniform_real_distribution<double> dis;
};
#endif
