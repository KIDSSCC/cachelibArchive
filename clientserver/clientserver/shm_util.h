#ifndef SHM_UTIL
#define SHM_UTIL

#include <iostream>
#include <vector>
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
#include <map>
#include <string>

//for timeval
#include <sys/time.h>

//for atomic_flag 
#include <atomic>


//对于注册缓存时的操作，先同时保留共享内存和消息队列的实现方式
//以消息队列为主

//消息队列配置
#define MSG_KEY 1412
#define MSG_KEY_SIZE 512

#define ADDPOOL_C 1
#define ADDPOOL_S 11

//共享内存配置
#define SHM_KEY_SIZE 512
#define SHM_VALUE_SIZE 1048576
#define SHM_ID_SIZE 32
#define SIG_CLOSE -1
#define SIG_SET 0
#define SIG_GET 1
#define SIG_DEL 2



//注册缓存池——共享内存结构
struct app_name
{
    int pid;
    char name[32];
};
//注册缓存池——消息队列结构
struct m_addpool_c
{
    long mtype;
    char name[MSG_KEY_SIZE];
    int pid;
};
//缓存操作结构
struct shm_stru
{
    int ctrl;
    int pid;
    char shmId[SHM_ID_SIZE];
    char key[SHM_KEY_SIZE];
    char value[SHM_VALUE_SIZE];
};

class CacheHitStatistics{
public:
	timeval startTime;
	timeval endTime;
	std::atomic_flag spinlock;
	std::atomic_flag spinlockForRate;
	std::vector<double> totalGet;
	std::vector<double> hitGet;
	std::string poolName;
	std::string logFileName;
	CacheHitStatistics(std::string name):spinlock(ATOMIC_FLAG_INIT), spinlockForRate(ATOMIC_FLAG_INIT){
		this->poolName = name;
		this->logFileName = name + "_cachehitrate.log";
		gettimeofday(&(this->startTime), NULL);
	};
	void adjustSize(int nSize){
		while(int(totalGet.size())<nSize+1){
			totalGet.push_back(0);	
			hitGet.push_back(0);
		}
	};

};

#endif
