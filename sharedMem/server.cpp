#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <set>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <fstream>

#include "cachelibHeader.h"
#include "shm_util.h"

#define MAX_WAIT 10000000
#define SIZE_CONV ((size_t)4 * 1024 * 1024)
using namespace std;
using namespace facebook::cachelib_examples;

int msgid;
map<string, int> poolRecord;
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



void sharedMemCtl(const char* appName)
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
                //cout<<"set operation---key is: "<<getMessage->key<<" and value is: "<<getMessage->value<<" pid is: "<<getMessage->pid<<endl;
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
			//cout<<"from server, Received: " << poolName << endl;
			int pid = addpool_(poolName);
			//cout<<"poolName is: "<<poolName<<" pid is: "<<pid<<endl;
			if(poolRecord[poolName]==0){
				poolRecord[poolName]=1;
				thread t(sharedMemCtl, poolName.c_str());
				t.detach();
			}
			int bytesSent = send(client_socket, &pid, sizeof(pid), 0);
			if(bytesSent == -1){
				cout<<"Error:failed to send response\n";
			}
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
