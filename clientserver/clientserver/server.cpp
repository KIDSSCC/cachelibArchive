#include <iostream>
#include <cstring>
#include <thread>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <cstdlib> 
#include <sys/time.h>
//for atomic_flag
#include <atomic>

#include "cachelibHeader.h"
#include "config.h"
#include "shm_util.h"
#include "dcp.h"

//Command line parameter -p -c -d
DEFINE_int32(p, -1, "Pool size");
DEFINE_int32(c, -1, "Cache size");
DEFINE_int32(d, 0, "Default pool");
DEFINE_int32(g, 0, "Granularity");


using namespace std;
using namespace facebook::cachelib_examples;

atomic_flag slockForRecord = ATOMIC_FLAG_INIT;
map<string, pair<int, int>> poolRecord;
map<string, CacheHitStatistics*> name2CHS;
size_t size_conv = SIZE_CONV;

Node* dcpNode = nullptr;

// timeval格式转seconds
double timeval_to_seconds(const timeval& t) 
{
    return t.tv_sec + t.tv_usec / 1000000.0;
}

// 计算两个时间点之间的间隔时间
double getUsedTime(const timeval& st,const timeval& en)
{
	double startTime=timeval_to_seconds(st);
	double endTime=timeval_to_seconds(en);
	return endTime-startTime;
}

// 检查当前所有缓存池的大小
map<string, uint64_t> getCacheStats()
{
	map<string, uint64_t> res;
	set<PoolId> allPool = getPoolIds_();
	for(const auto& pid:allPool)
	{
		PoolStats currPoolStat = getPoolStat(pid);
		res[currPoolStat.poolName] = currPoolStat.poolSize / size_conv;
	}	
	return res;
}

// 从string类型中解析出新的缓存池划分方案，并执行新的方案
void executeNewConfig(string config)
{
	istringstream iss(config);
	string line,size;
	getline(iss, line);
	getline(iss, size);
	istringstream line_stream1(line);
	istringstream line_stream2(size);
	string token;
	size_t num;

	vector<string> poolNames;
	vector<size_t> poolSizes;
	while(line_stream1>>token && line_stream2>>num){
		poolNames.push_back(token);
		poolSizes.push_back(num);
	}
	// first,shrinkle pool
	for(int i=0;i<(int)poolNames.size();i++){
		size_t currSize = getPoolSizeFromName(poolNames[i]) / size_conv;
		if(currSize>poolSizes[i]){
			resizePool(poolNames[i], poolSizes[i] * size_conv);
		}
	}
	// second, grow pool
	for(int i=0;i<(int)poolNames.size();i++){
		size_t currSize = getPoolSizeFromName(poolNames[i]) / size_conv;
		if(currSize<poolSizes[i]){
			resizePool(poolNames[i], poolSizes[i] * size_conv);
		}
	}
}

void sharedMemCtl(string appName, int no, CacheHitStatistics* chs)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    string localAppName = appName;
	// XLOG(INFO) << "Register SHM: " << localAppName;
    // create new shared memory
    int shm_fd = shm_open(localAppName.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) 
    {
		XLOG(ERR) << "Error opening shared memory";
        exit(EXIT_FAILURE);
    }
    // adjust the size of shared memory
    if (ftruncate(shm_fd, SHARED_MEMORY_SIZE) == -1) 
    {
		XLOG(ERR) << "Error resizing shared memory";
        exit(EXIT_FAILURE);
    }
    // map the shared memory to the process memory
    void* shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) 
    {
		XLOG(ERR) << "Error mapping shared memory";
        exit(EXIT_FAILURE);
    }
	shm_stru *getMessage = static_cast<shm_stru*>(shared_memory);

    //create new semaphore
    string sem_server=localAppName + "_Server";
    string sem_getback=localAppName + "_getback";
	string getValue;

    //indicates whether the server currently needs to handle the shared memory area
    sem_t* semaphore=sem_open(localAppName.c_str(), O_CREAT, 0666,0);
    //indicates whether the shared memory is free to begin a new request
    sem_t* semaphore_Server=sem_open(sem_server.c_str(), O_CREAT, 0666,1);
    //indicates whether the client currently can read the result of get operation
    sem_t* semaphore_GetBack=sem_open(sem_getback.c_str(), O_CREAT, 0666,0);
    
    bool available = true;
    while(available)
    {
    	int waitCount=0;
        //try_wait() within a certain number of times or wait()
		while(sem_trywait(semaphore)!=0){
			//can't get semaphore
			if(waitCount>MAX_WAIT){
				XLOG(WARNING) << localAppName << " Begin Sleeping";
				sem_wait(semaphore);
				XLOG(WARNING) << localAppName << " Be Awakened";
				break;
			}
			waitCount++;
		}
		bool res =false;
        switch(getMessage->ctrl)
        {
            case SIG_SET:
                // set operation
                set_(getMessage->pid,getMessage->key,getMessage->value);
                sem_post(semaphore_Server);
                break;
            case SIG_GET:
                // get operation 
                res = get_(getMessage->key, getMessage->value);
				#if CACHE_HIT
					// 如果本地查询miss，通过dcp向远程节点发起请求。
					if(!res){
						string key2find(getMessage->key);
						key2find = "0:" + key2find;
						string remoteRes = dcpNode->sendMessage(key2find);
						
						// 将远程查询的结果拷贝至共享内存区
						strcpy(getMessage->value, remoteRes.c_str());
					}
					// while(chs->spinlockForRate.test(memory_order_acquire));
					// chs->totalGet[no]++;
					// if(res){
					// 	chs->hitGet[no]++;
					// }
					// chs->spinlockForRate.clear(memory_order_release);
				#endif
                sem_post(semaphore_GetBack);
                break;
            case SIG_DEL:
                //del操作
                del_(getMessage->key);
                sem_post(semaphore_Server);
	    	case SIG_CLOSE:
				sem_post(semaphore_Server);
				available = false;
				break;
            default:
                break;
        }
		#if CACHE_HIT
		// 统计命中信息
			if(!chs->spinlock.test_and_set(memory_order_acquire)){
				gettimeofday(&(chs->endTime), NULL);
				if(getUsedTime(chs->startTime, chs->endTime)>10){
					while(chs->spinlockForRate.test_and_set(memory_order_acquire));
					double t_totalGet = 0;
					double t_totalHit = 0;
					for(int i = 0; i<int(chs->totalGet.size()); i++){
						t_totalGet += chs->totalGet[i];
						chs->totalGet[i] = 0;
						t_totalHit += chs->hitGet[i];
						chs->hitGet[i] = 0;
					}
					chs->spinlockForRate.clear(memory_order_release);
					ofstream logFile(chs->logFileName, ios::app);
					if(logFile.is_open()){
						//string logInfo = "totalGet is: " + to_string(t_totalGet) + " totalHit is: " + to_string(t_totalHit) +  " Cache Hit Rate: " + to_string(t_totalHit/t_totalGet);
						string logInfo = "Cache Hit Rate: " + to_string(t_totalHit/t_totalGet);
						logFile<<logInfo<<endl;
						logFile.close();
					}
					gettimeofday(&(chs->startTime), NULL);
				}
				chs->spinlock.clear(memory_order_release);
			}
		#endif
    }
	
	// release shared memory and semaphore,  file_descriptor specially,
	munmap(shared_memory, SHARED_MEMORY_SIZE);
	close(shm_fd);
	shm_unlink(localAppName.c_str());

	sem_close(semaphore);
	sem_close(semaphore_Server);
	sem_close(semaphore_GetBack);
	sem_unlink(localAppName.c_str());
	sem_unlink(sem_server.c_str());
	sem_unlink(sem_getback.c_str());

	while(slockForRecord.test_and_set(memory_order_acquire));
	poolRecord[chs->poolName].first--;
	slockForRecord.clear(memory_order_release);

	// XLOG(INFO) << "Close SHM: " << localAppName;
	return;
}

void listen_addpool()
{
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket == -1){
		XLOG(ERR) << "Error: Failed to create socket";
		return;
	}
	//bind address and port
	sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(54000);
	int yes = 1;
	// allow socket reuse shortly after it is closed
	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))==-1){
		XLOG(ERR) << "Error: Failed to set socket";
		return;
	}
	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
		XLOG(ERR) << "Error: Failed to bind";
		return ;
	}
	//listen the connection
	if (listen(server_socket, SOMAXCONN) == -1) {
		XLOG(ERR) << "Error: Failed to listen";
		return ;
    }
	
    while(true){
		int client_socket = accept(server_socket, nullptr,nullptr);
		if(client_socket == -1){
			XLOG(ERR) << "Error: Failed to accept";
			continue;
		}
		char buf[1024];
		memset(buf, 0 ,sizeof(buf));
		//read the client message
		int bytesReceived = recv(client_socket, buf, sizeof(buf), 0);
		if(bytesReceived == -1){
			XLOG(ERR) << "Error: Failed to receive data";
			close(client_socket);
			continue;
		}
		// wait to solve request
		if(buf[0]=='A'){
			//registre pool
			string poolName = string(buf, 2, bytesReceived-2);
			int pid = addpool_(poolName);
			//for multithread
			int newShmId;
			auto it = poolRecord.find(poolName);
			if(it == poolRecord.end()||it->second.first==0){
				//new cache pool
				while(slockForRecord.test_and_set(memory_order_acquire));
				poolRecord[poolName] = make_pair(1, 0);
				newShmId = (poolRecord[poolName].second)++;
				slockForRecord.clear(memory_order_release);
				name2CHS[poolName] = new CacheHitStatistics(poolName);
			}else{
				while(slockForRecord.test_and_set(memory_order_acquire));
				(poolRecord[poolName].first)++;
				newShmId = (poolRecord[poolName].second)++;
				slockForRecord.clear(memory_order_release);
			}
			name2CHS[poolName]->adjustSize(newShmId);
			string shmId = poolName + "_" + to_string(newShmId);
			thread t(sharedMemCtl, shmId, newShmId, name2CHS[poolName]);
			t.detach();

			string sendInfo = to_string(pid) + " " + shmId;
			int bytesSent = send(client_socket, sendInfo.c_str(), sendInfo.size(), 0);
			if(bytesSent == -1){
				XLOG(ERR) << "Error: Failed to send response";
			}
			
		}else if(buf[0]=='G'){
			//get cache pool status
			map<string, uint64_t> poolStats = getCacheStats();
			ostringstream oss;
			for(const auto& pair:poolStats){
				oss<<pair.first<<":"<<pair.second<<";";
			}
			string serialized_map = oss.str();
			int bytesSent = send(client_socket, serialized_map.c_str(),serialized_map.size(), 0);
			if(bytesSent == -1){
				XLOG(ERR) << "Error: Failed to send response";
			}
		}else if(buf[0]=='S'){
			//set Status
			string getMessage = string(buf, 2, bytesReceived-2);
			executeNewConfig(getMessage);

		}else if(buf[0]=='E'){
			//end server
			close(client_socket);
			close(server_socket);
			break;
		}
		close(client_socket);
	}

}

bool getPack(const std::string& param1, char* param2){
	return get_(param1, param2);
}


int main(int argc, char* argv[])
{
	folly::Init init(&argc, &argv);

	int poolSize = -1;
	int cacheSize = -1;
	int defaultPool = 0;
	size_t g_tmp = 0;
	cacheSize = FLAGS_c;
	poolSize = FLAGS_p;
	defaultPool = FLAGS_d;
	g_tmp = FLAGS_g;

	size_conv = g_tmp==0?size_conv:(g_tmp * MB_SIZE);

    initializeCache(cacheSize, poolSize, defaultPool);
    
    thread t_listenAddPool(listen_addpool);
    t_listenAddPool.join();

	//分布式通信设置
	vector<string> other_nodes = {OTHER_NODE};
	dcpNode = new Node(DCP_PORT, other_nodes, getPack);
	dcpNode->start();

    destroyCache();
}
