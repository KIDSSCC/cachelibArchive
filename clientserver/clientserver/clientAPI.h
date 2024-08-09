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

#include "shm_util.h"
#include <random>


using namespace std;
class CachelibClient
{
private:
    int pid;
    int msgid;
    string prefix;

    int shm_fd;
    void* shared_memory;
	string shmId;
    sem_t* semaphore;
    sem_t* semaphore_Server;
    sem_t* semaphore_GetBack;

    //接收get操作返回结果
    char getValue[SHM_VALUE_SIZE];
public:
    CachelibClient();
    ~CachelibClient();
    void prepare_shm(string appName);
    int addpool(string poolName);
    void setKV(string key,string value);
    string getKV(string key);
    bool delKV(string key);

    //util
    int getPid(){return this->pid;};
    int getHit;


    //random
    random_device rd;
    mt19937 gen;
    uniform_real_distribution<double> dis;
};

class TailLatency {
private:
	// when measure tail latency at a specific operations ,use greater,otherwise, use less
	priority_queue<long long, vector<long long>, greater<long long>> max_heap;
	int size;
public:
	TailLatency(int k) : size(k) {}
	void push(long long num){
		if(int(max_heap.size())<size){
			max_heap.push(num);
		}else{
			if(num>max_heap.top()){
				max_heap.pop();
				max_heap.push(num);
			}
		}
		//max_heap.push(num);
	}
	long long getResult(){
		return max_heap.top();
		/*
		int totalSize = max_heap.size();
		int p999 = totalSize * 0.001;
		int p95 = totalSize * 0.05;
		int p50 = totalSize * 0.5;
		int count = 0;
		while(count<p999){
			max_heap.pop();
			count++;
		}
		cout<<"P999 is: "<<max_heap.top()<<endl;
		while(count<p95){
			max_heap.pop();
			count++;
		}
		cout<<"P95 is: "<<max_heap.top()<<endl;
		while(count<p50){
			max_heap.pop();
			count++;
		}
		cout<<"P50 is: "<<max_heap.top()<<endl;
		return 0;
		*/
	}
	void clear(){
		priority_queue<long long,vector<long long>,greater<long long>>().swap(max_heap);
	}

};
#endif
