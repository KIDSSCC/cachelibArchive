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
#include <vector>

#include "cachelibHeader.h"
#include "messageInfo.h"

#define SHM_KEY_SIZE 512
#define SHM_VALUE_SIZE 1024
struct shm_stru
{
    int ctrl;
    int pid;
    char key[SHM_KEY_SIZE];
    char value[SHM_VALUE_SIZE];
};

using namespace std;
using namespace facebook::cachelib_examples;

int msgid;
vector<thread*> thread_pool;

void initSem(const char* sem_name,sem_t** semaphore, int initValue)
{
    *semaphore = sem_open(sem_name, 0);
    if (*semaphore != SEM_FAILED) {
        // 信号量已经存在，关闭它
        sem_close(*semaphore);

        // 删除信号量
        sem_unlink(sem_name);
    }
    *semaphore = sem_open(sem_name, O_CREAT, 0666, initValue);
    if (*semaphore == SEM_FAILED) {
        perror(sem_name);
        exit(EXIT_FAILURE);
    }
    return;
}
void sharedMemCtl(char* appName)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    string sem_server=string(appName)+"_Server";
    string sem_getback=string(appName)+"_getback";
    //创建共享内存
    int shm_fd = shm_open(appName, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) 
    {
        perror("Error creating shared memory");
        exit(EXIT_FAILURE);
    }
    //调整共享内存区大小
    if (ftruncate(shm_fd, SHARED_MEMORY_SIZE) == -1) 
    {
        perror("Error resizing shared memory");
        exit(EXIT_FAILURE);
    }
    // 将共享内存映射到进程的地址空间
    void* shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) 
    {
        perror("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }
    //创建信号量
    
    sem_t* semaphore=sem_open(appName, O_CREAT, 0666,0);
    sem_t* semaphore_Server=sem_open(sem_server.c_str(), O_CREAT, 0666,1);
    sem_t* semaphore_GetBack=sem_open(sem_getback.c_str(), O_CREAT, 0666,0);
    string getValue;
    while(true)
    {
        //锁资源
        sem_wait(semaphore);
        shm_stru *getMessage = static_cast<shm_stru*>(shared_memory);
        switch(getMessage->ctrl)
        {
            case 0:
                //set操作
                set_(getMessage->pid,getMessage->key,getMessage->value);
                //释放资源
                sem_post(semaphore_Server);
                break;
            case 1:
                //get操作
                getValue=get_(getMessage->key);
                strcpy(getMessage->value,getValue.c_str());
                sem_post(semaphore_GetBack);
                break;
            case 2:
                //del操作
                del_(getMessage->key);
                sem_post(semaphore_Server);
            default:
                break;
        }
    }
    
}
void listen_addpool()
{
    while(1)
    {
        m_addpool_c rcv;
        if(msgrcv(msgid, &rcv, sizeof(m_addpool_c)-sizeof(long),ADDPOOL_C,0)==-1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        rcv.pid=addpool_(rcv.name);
        thread t(sharedMemCtl,rcv.name);
        t.detach();
        rcv.mtype = ADDPOOL_S;
        if(msgsnd(msgid, &rcv, sizeof(m_addpool_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        
    }
    cout<<"can't access here\n";
}
void MQInit()
{
    msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        // 当前队列不存在，创建新队列
        msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
        if (msgid == -1) {
            perror("msgget");
            exit(EXIT_FAILURE);
        }
    } else {
        // 当前队列存在，重新启动
        if (msgctl(msgid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            exit(EXIT_FAILURE);
        }
        msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
        if (msgid == -1) {
            perror("msgget");
            exit(EXIT_FAILURE);
        }
    }
}
int main(int argc, char** argv)
{
    folly::Init init(&argc, &argv);
    initializeCache();

    MQInit();
    thread t1(listen_addpool);
    t1.join();

    destroyCache();
}