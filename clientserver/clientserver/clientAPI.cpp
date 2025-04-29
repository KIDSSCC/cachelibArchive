#include "clientAPI.h"
#include <fstream>
#include <chrono>

/*
CachelibClient：构造函数，初始化随机数生成引擎
*/
CachelibClient::CachelibClient():gen(rd()), dis(0.0, 1.0)
{
    this->pid = -1;
	this->shmId = "";
}

/*
~CachelibClient：析构函数，向server发送中止信号并释放共享内存与信号量资源。
*/
CachelibClient::~CachelibClient(){
	if(this->shmId != ""){
		//等待Server端空闲，向Server端发送SIG_CLOSE信号关闭连接
		while(sem_trywait(this->semaphore_Server)!=0);
		shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
		message->ctrl=SIG_CLOSE;
		sem_post(this->semaphore);

    	// release shared memory
    	int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    	munmap(this->shared_memory, SHARED_MEMORY_SIZE);
    	close(this->shm_fd);
    	// release semaphore
    	string sem_server = this->shmId + "_Server";
    	string sem_getback = this->shmId + "_getback";
    	sem_close(this->semaphore);
		sem_close(this->semaphore_Server);
		sem_close(this->semaphore_GetBack);
	}
}

/*
prepare_shm:根据标识符进行共享内存映射，并打开相关信号量
params：
    appName:标识符，共享内存标识为appName，三个信号量标识分别为appName，appName_Server，appName_getback
*/
void CachelibClient::prepare_shm(string appName)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    
    //打开共享内存
    do
    {
        this->shm_fd = shm_open(appName.c_str(), O_RDWR, 0666);
    } while (this->shm_fd == -1);
    // 将共享内存映射到进程的地址空间
    this->shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, this->shm_fd, 0);
    if (this->shared_memory == MAP_FAILED) 
    {
        perror("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }
    //打开信号量
    string sem_server=appName+"_Server";
    string sem_getback=appName+"_getback";
    do{
        this->semaphore = sem_open(appName.c_str(), 0);
    } while (this->semaphore==SEM_FAILED);
    do{
        this->semaphore_Server = sem_open(sem_server.c_str(), 0);
    } while (this->semaphore_Server==SEM_FAILED);
    do{
        this->semaphore_GetBack = sem_open(sem_getback.c_str(), 0);
    } while (this->semaphore_GetBack==SEM_FAILED);


    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    strcpy(message->shmId, appName.c_str());
    return;
}

/*
addpool:与服务端之间以socket通信的形式注册新的缓存池，获取缓存池id与同步标识
params：
    poolName：字符串类型的缓存池名
returns：
    int：缓存池id
*/
int CachelibClient::addpool(string poolName)
{
    // 错误检查：同一client对象禁止复用
    if(this->pid!=-1){
        cout<<"Error: Cache client already used\n";
        exit(EXIT_FAILURE);
    }
    // socket通信准备
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket == -1){
		cout<<"Error: Failed to create socket\n";
        exit(EXIT_FAILURE);
	}
	sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(54000);
	inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
	if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
		cout<<"Error: Failed to connect to server\n";
        exit(EXIT_FAILURE);
	}

    // 向服务端发送信息
	string message = "A:" + poolName;
	int bytesSent = send(client_socket, message.c_str(), message.size() + 1, 0);
	if (bytesSent == -1){
		cout<<"Error: Failed to send message\n";
		close(client_socket);
		exit(EXIT_FAILURE);
	}

    // 接收返回结果
	char buffer[32];
	memset(buffer, 0, sizeof(buffer));
	int bytesReceived = recv(client_socket, buffer, 32, 0);
	if (bytesReceived == -1){
		cout<<"Error: Failed to recv response\n";
		close(client_socket);
		exit(EXIT_FAILURE);
	}
	close(client_socket);

    // 返回结果中解析pid与标识符。标识符用于初始化自身共享内存等内容
	string recvInfo = buffer;
	size_t spacePosition = recvInfo.find(' ');
	this->pid = stoi(recvInfo.substr(0, spacePosition));
	this->shmId = recvInfo.substr(spacePosition+1);

	this->prefix = poolName + "_";
	prepare_shm(this->shmId);
	return this->pid;
}

/*
haspool：进行访存操作前检查是否已注册缓存池
*/
void CachelibClient::haspool()
{
    if(this->pid == -1)
    {
        // 未注册缓存池进行访存调用，进行错误提示
        cout<<"Error: G/S/D without cache pool\n";
        exit(EXIT_FAILURE);
    }
}

/*
setKV：向缓存中存入
*/
void CachelibClient::setKV(string& key, const string& value)
{
    this->haspool();
    //锁资源
    while(sem_trywait(this->semaphore_Server)!=0);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=SIG_SET;
    message->pid=this->pid;
    // memset(message->key,0,sizeof(message->key));
    // memset(message->value,0,sizeof(message->value));

    string fullKey = this->prefix+key;
    strcpy(message->key,fullKey.c_str());
    strcpy(message->value,value.c_str());
    
    //释放资源
    sem_post(this->semaphore);
    return ;
}

/*
getKV：根据key在缓存池内查询value
params：
    key：待查询的key
*/
string CachelibClient::getKV(const string& key)
{
    this->haspool();
    //锁资源
    while(sem_wait(this->semaphore_Server)!=0);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=SIG_GET;
    // memset(message->key,0,sizeof(message->key));
    // memset(message->value,0,sizeof(message->value));

    strcpy(message->key,(this->prefix+key).c_str());
    sem_post(this->semaphore);

    //等待回传
    while(sem_trywait(this->semaphore_GetBack)!=0);

    //control cache miss
    double randomNum = dis(gen);
    if(randomNum>=(double)1 - HIT_CONTROL){
        // 正常返回
        string cache_ans = message->value;
    	sem_post(this->semaphore_Server);
    	return cache_ans;
    }else{
        // 模拟miss
	    sem_post(this->semaphore_Server);
	    return "";
    }
}

/*
delKV：根据key删除缓存池内的value
params：
    key：待删除的key
*/
bool CachelibClient::delKV(string& key)
{
    this->haspool();
    //锁资源
    while(sem_trywait(this->semaphore_Server)!=0);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=SIG_DEL;
    strcpy(message->key,(this->prefix+key).c_str());
    //释放资源
    sem_post(this->semaphore);
    return true;
}
