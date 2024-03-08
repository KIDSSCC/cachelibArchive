#ifndef SHM_UTIL
#define SHM_UTIL

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
#include <map>
#include <string>


//对于注册缓存时的操作，先同时保留共享内存和消息队列的实现方式
//以消息队列为主

//消息队列配置
#define MSG_KEY 1412
#define MSG_KEY_SIZE 512

#define ADDPOOL_C 1
#define ADDPOOL_S 11

//共享内存配置
#define SHM_KEY_SIZE 512
#define SHM_VALUE_SIZE 1024
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
    char key[SHM_KEY_SIZE];
    char value[SHM_VALUE_SIZE];
};

class CacheStat{
public:
	size_t availableSize;
	std::map<std::string, size_t> allPoolSize;
	
	void printCacheStat(){
		std::cout<<"availableSize: "<<this->availableSize<<std::endl;
		std::cout<<"allPoolSize: "<<std::endl;
		for(const auto& pair:allPoolSize){
			std::cout<<"[ "<<pair.first<<", "<<pair.second<<" ]"<<std::endl;
		}

	}

};



#endif
