#ifndef CLIENT_API_SHM
#define CLIENT_API_SHM

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <thread>

#include "messageInfo.h"


#define SHM_KEY_SIZE 512
#define SHM_VALUE_SIZE 1024

using namespace std;
struct shm_stru
{
    int ctrl;
    int pid;
    char key[SHM_KEY_SIZE];
    char value[SHM_VALUE_SIZE];
};

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
public:
    CachelibClient();
    void prepare_shm(string appName);
    int addpool(string poolName);
    bool setKV(string key,string value);
    string getKV(string key);
    bool delKV(string key);
    //util
    int getPid(){return this->pid;};

    int getHit;
};
#endif
