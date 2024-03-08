#include <iostream>
#include <thread>
#include <vector>
#include <set>

#include "cachelibHeader.h"
#include "shm_util.h"

#define MAX_WAIT 10000000
using namespace std;
using namespace facebook::cachelib_examples;

int msgid;

void getCacheStats()
{
	CacheStat cacheStats;
	cacheStats.availableSize = getAvailableSize();
	set<PoolId> allPool = getPoolIds_();
	for(const auto& pid:allPool){
		PoolStats currPoolStat = getPoolStat(pid);
		cacheStats.allPoolSize[currPoolStat.poolName]=currPoolStat.poolSize;
	}	
	cacheStats.printCacheStat();
	return ;
}



void sharedMemCtl(char* appName)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
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
    string sem_server=string(appName)+"_Server";
    string sem_getback=string(appName)+"_getback";
    //指示当前是否需要服务端处理共享内存区
    sem_t* semaphore=sem_open(appName, O_CREAT, 0666,0);
    //指示当前共享内存区是否空闲，客户端能够开始新的操作
    sem_t* semaphore_Server=sem_open(sem_server.c_str(), O_CREAT, 0666,1);
    //对于get操作的回传信号
    sem_t* semaphore_GetBack=sem_open(sem_getback.c_str(), O_CREAT, 0666,0);
    string getValue;
    while(true)
    {
    	int waitCount=0;

        //try_wait() within a certain number of times or wait()
	while(sem_trywait(semaphore)!=0)
	{
		//can't get semaphore
		if(waitCount>MAX_WAIT)
		{
			sem_wait(semaphore);
			break;
		}
		waitCount++;
	}
        //while(sem_trywait(semaphore)!=0);
        //sem_wait(semaphore);
        shm_stru *getMessage = static_cast<shm_stru*>(shared_memory);
        switch(getMessage->ctrl)
        {
            case 0:
                //set操作
                set_(getMessage->pid,getMessage->key,getMessage->value);
                //cout<<"set operation---key is: "<<getMessage->key<<" and value is: "<<getMessage->value<<endl;
                //释放资源
                sem_post(semaphore_Server);
                break;
            case 1:
                //get操作
                getValue=get_(getMessage->key);
                strcpy(getMessage->value,getValue.c_str());
                //cout<<"get operation---key is: "<<getMessage->key<<" and value is: "<<getMessage->value<<endl;
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
        //pid回传
        if(msgsnd(msgid, &rcv, sizeof(m_addpool_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        
    }
}
/*
void listen_addpool()
{
    char tmpaddr[]="tmp_add_pool_shm";
    int SHARED_MEMORY_SIZE = sizeof(app_name);
    //创建共享内存
    int shm_fd = shm_open(tmpaddr, O_CREAT | O_RDWR, 0666);
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
    string sema_client_name=string(tmpaddr)+"_client";
    string sema_write_right_name=string(tmpaddr)+"_write";
    string sema_add_pool_back_name=string(tmpaddr)+"_back";
    sem_t* sema_client=sem_open(sema_client_name.c_str(), O_CREAT, 0666,0);
    sem_t* sema_write_right=sem_open(sema_write_right_name.c_str(), O_CREAT, 0666,1);
    sem_t* sema_add_pool_back=sem_open(sema_add_pool_back_name.c_str(),O_CREAT, 0666,0);
    char* copiedName = (char*)malloc(32); 
    while(true)
    {
        sem_wait(sema_client);
        app_name *getMessage = static_cast<app_name*>(shared_memory);
        getMessage->pid=addpool_(getMessage->name);
        memset(copiedName,0,sizeof(copiedName));
        strcpy(copiedName,getMessage->name);
        thread t(sharedMemCtl,copiedName);
        t.detach();
        sem_post(sema_add_pool_back);
    }

}*/
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
    listen_addpool();

    destroyCache();
}
