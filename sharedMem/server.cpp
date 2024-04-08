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
//for timeval
#include <sys/time.h>
//for atomic_flag
#include <atomic>

#include "cachelibHeader.h"
#include "shm_util.h"

#define MAX_WAIT 100000000
#define SIZE_CONV ((size_t)4 * 1024 * 1024)
using namespace std;
using namespace facebook::cachelib_examples;

int msgid;
atomic_flag slockForRecord = ATOMIC_FLAG_INIT;
map<string, pair<int, int>> poolRecord;
map<string, CacheHitStatistics*> name2CHS;

double timeval_to_seconds(const timeval& t) {
    return t.tv_sec + t.tv_usec / 1000000.0;
}
double getUsedTime(const timeval& st,const timeval& en)
{
	double startTime=timeval_to_seconds(st);
	double endTime=timeval_to_seconds(en);
	return endTime-startTime;
}
map<string, uint64_t> getCacheStats()
{
	map<string, uint64_t> res;
	set<PoolId> allPool = getPoolIds_();
	for(const auto& pid:allPool){
		PoolStats currPoolStat = getPoolStat(pid);
		//cacheStats.allPoolSize[currPoolStat.poolName]=currPoolStat.poolSize;
		res[currPoolStat.poolName] = currPoolStat.poolSize / SIZE_CONV;
		//cout<<"Name is: "<<currPoolStat.poolName<<" size is: "<<res[currPoolStat.poolName]<<endl;
	}	
	//cacheStats.printCacheStat();
	return res;
}

void executeNewConfig(string config){
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
	for(int i=0;i<poolNames.size();i++){
		size_t currSize = getPoolSizeFromName(poolNames[i])/SIZE_CONV;
		if(currSize>poolSizes[i]){
			resizePool(poolNames[i], poolSizes[i] * SIZE_CONV);
		}
	}
	// second, grow pool
	for(int i=0;i<poolNames.size();i++){
		size_t currSize = getPoolSizeFromName(poolNames[i])/SIZE_CONV;
		if(currSize<poolSizes[i]){
			resizePool(poolNames[i], poolSizes[i] * SIZE_CONV);
		}
	}
}



void sharedMemCtl(const char* appName, int no, CacheHitStatistics* chs)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    string localAppName = appName;
    cout<<"register SHM: "<<localAppName<<endl;
    //创建共享内存
    int shm_fd = shm_open(localAppName.c_str(), O_CREAT | O_RDWR, 0666);
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
    string sem_server=localAppName + "_Server";
    string sem_getback=localAppName + "_getback";
    //指示当前是否需要服务端处理共享内存区
    sem_t* semaphore=sem_open(localAppName.c_str(), O_CREAT, 0666,0);
    //指示当前共享内存区是否空闲，客户端能够开始新的操作
    sem_t* semaphore_Server=sem_open(sem_server.c_str(), O_CREAT, 0666,1);
    //对于get操作的回传信号
    sem_t* semaphore_GetBack=sem_open(sem_getback.c_str(), O_CREAT, 0666,0);
    string getValue;


    bool available = true;
    while(available)
    {
    	int waitCount=0;

        //try_wait() within a certain number of times or wait()
	while(sem_trywait(semaphore)!=0)
	{
		//can't get semaphore
		if(waitCount>MAX_WAIT)
		{
			cout<<"begin sleep\n";
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
            case SIG_SET:
                //set操作
                set_(getMessage->pid,getMessage->key,getMessage->value);
                //cout<<"set operation---key is: "<<getMessage->key<<" and value is: "<<getMessage->value<<" pid is: "<<getMessage->pid<<endl;
                //释放资源
                sem_post(semaphore_Server);
                break;
            case SIG_GET:
                //get操作
                getValue=get_(getMessage->key);
		while(chs->spinlockForRate.test(memory_order_acquire));
		chs->totalGet[no]++;
		if(getValue != ""){
			chs->hitGet[no]++;
		}
                strcpy(getMessage->value,getValue.c_str());
                //cout<<"get operation---key is: "<<getMessage->key<<" and value is: "<<getMessage->value<<endl;
                sem_post(semaphore_GetBack);
                break;
            case SIG_DEL:
                //del操作
                del_(getMessage->key);
                sem_post(semaphore_Server);
	    case SIG_CLOSE:
		sem_post(semaphore_Server);
		while(slockForRecord.test_and_set(memory_order_acquire));
		poolRecord[chs->poolName].first--;
		slockForRecord.clear(memory_order_release);

		available = false;
		break;

            default:
                break;
        }
	//if(!chs->spinlock.test_and_set(memory_order_acquire)){
	if(no == 0){
		gettimeofday(&(chs->endTime), NULL);
		if(getUsedTime(chs->startTime, chs->endTime)>2){
			//cout<<"name is: "<<localAppName<<" and time is: "<<getUsedTime(chs->startTime, chs->endTime)<<endl;
			while(chs->spinlockForRate.test_and_set(memory_order_acquire));
			double t_totalGet = 0;
			double t_totalHit = 0;
			for(int i = 0; i<chs->totalGet.size(); i++){
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
	
    }
	munmap(shared_memory, SHARED_MEMORY_SIZE);
	shm_unlink(localAppName.c_str());
	sem_close(semaphore);
	sem_close(semaphore_Server);
	sem_close(semaphore_GetBack);
	sem_unlink(localAppName.c_str());
	sem_unlink(sem_server.c_str());
	sem_unlink(sem_getback.c_str());
	cout<<"close SHM: "<<localAppName<<endl;
	return;
}

void listen_addpool()
{
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket == -1){
		cout<<"Error: Failed to create socket\n";
		return;
	}
	//bind address and port
	sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(54000);
	int yes = 1;
	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))==-1){
		cout << "Error: Failed to set socket\n";
		return;
	}
	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
		cout << "Error: Failed to bind\n";
		return ;
	}
	//listen the connection
	if (listen(server_socket, SOMAXCONN) == -1) {
		cout << "Error: Failed to listen\n";
		return ;
    	}
	
    	while(true){
		int client_socket = accept(server_socket, nullptr,nullptr);
		if(client_socket == -1){
			cout<<"Failed to accept\n";
			continue;
		}
		char buf[1024];
		memset(buf, 0 ,sizeof(buf));
		//read the client message
		int bytesReceived = recv(client_socket, buf, sizeof(buf), 0);
		if(bytesReceived == -1){
			cout<<"Error: failed to receive data\n";
			close(client_socket);
			continue;
		}
		/* wait to solve request*/
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
				name2CHS[poolName]->adjustSize(newShmId);
				
			}else{
				while(slockForRecord.test_and_set(memory_order_acquire));
				(poolRecord[poolName].first)++;
				newShmId = (poolRecord[poolName].second)++;
				slockForRecord.clear(memory_order_release);

				name2CHS[poolName]->adjustSize(newShmId);
				
			}
			string shmId = poolName + "_" + to_string(newShmId);
			thread t(sharedMemCtl, shmId.c_str(), newShmId, name2CHS[poolName]);
			t.detach();

			string sendInfo = to_string(pid) + " " + shmId;
			int bytesSent = send(client_socket, sendInfo.c_str(), sendInfo.size(), 0);
			if(bytesSent == -1){
				cout<<"Error:failed to send response\n";
			}

			
			/*
			if(poolRecord[poolName]==0){
				poolRecord[poolName]=1;
				thread t(sharedMemCtl, poolName.c_str());
				t.detach();
			}
			int bytesSent = send(client_socket, &pid, sizeof(pid), 0);
			*/
			
		}else if(buf[0]=='G'){
			//cout<<"get a request to get pool Stats\n";
			//get Status
			map<string, uint64_t> poolStats = getCacheStats();
			ostringstream oss;
			for(const auto& pair:poolStats){
				oss<<pair.first<<":"<<pair.second<<";";
			}
			string serialized_map = oss.str();
			int bytesSent = send(client_socket, serialized_map.c_str(),serialized_map.size(), 0);
			if(bytesSent == -1){
				cout<<"Error:Failed to send response\n";
			}
		}else if(buf[0]=='S'){
			//set Status
			string getMessage = string(buf, 2, bytesReceived-2);
			//cout<<"getMessage: "<<getMessage<<endl;
			executeNewConfig(getMessage);

		}
		close(client_socket);
	}

	/*
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
        
    */
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
void listen_CacheStats(){
	/*
	string poolName_1 = "pool1_";
	string poolName_2 = "pool2_";
	string poolName_3 = "pool3_";
	int pid1 = addpool_(poolName_1);
	int pid2 = addpool_(poolName_2);
	int pid3 = addpool_(poolName_3);
	*/
	
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket == -1){
		cout<<"Error creating socket"<<endl;
		return;
	}
	//bind host and port
	sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(1412);
	server_address.sin_addr.s_addr = INADDR_ANY;
	if(bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1){
		cout<<"Error binding socket"<<endl;
		close(server_socket);
		return;
	}
	//listen the connection
	if(listen(server_socket, 5)==-1){
		cout<<"Error listening on socket"<<endl;
		close(server_socket);
		return;
	}
	
	//accept the connection
	sockaddr_in client_address;
	socklen_t client_address_size = sizeof(client_address);
	int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_size);
	if(client_socket == -1){
		cout<<"Error accepting connection"<<endl;
		close(server_socket);
	}

	char buffer[1024];
	int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
	if(bytes_received == -1){
		cout<<"Error receiving data"<<endl;
		close(client_socket);
		close(server_socket);
		return;
	}
	buffer[bytes_received] = '\0';
	cout<<"Received: "<<buffer<<endl;

	//send response
	const char* response_message = "Hello from C++";
	send(client_socket, response_message, strlen(response_message), 0);

	//close socket
	close(client_socket);
	close(server_socket);

	return;
		


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
    
    thread t_listenAddPool(listen_addpool);
    //thread t_listenCacheStat(listen_CacheStats);

    

    t_listenAddPool.join();
    //t_listenCacheStat.join();

    destroyCache();
}
