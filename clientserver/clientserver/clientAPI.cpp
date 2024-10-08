#include "clientAPI.h"


CachelibClient::CachelibClient():gen(rd()), dis(0.0, 1.0)
{
    this->getHit=0;
	this->shmId = "";
}
CachelibClient::~CachelibClient(){
	if(this->shmId != ""){
		while(sem_trywait(this->semaphore_Server)!=0);
		//prepare SIG_CLOSE
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

void CachelibClient::prepare_shm(string appName)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    
    //打开共享内存
    do
    {
        this->shm_fd = shm_open(appName.c_str(), O_RDWR, 0666);
    } while (this->shm_fd == -1);
	//std::cout<<"after get shm\n";
    // 将共享内存映射到进程的地址空间
    this->shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, this->shm_fd, 0);
	//std::cout<<"after map\n";
    if (this->shared_memory == MAP_FAILED) 
    {
        perror("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }
    //打开信号量
    string sem_server=string(appName)+"_Server";
    string sem_getback=string(appName)+"_getback";
    do{
        this->semaphore = sem_open(appName.c_str(), 0);
    } while (this->semaphore==SEM_FAILED);
	//std::cout<<"after get sema 1\n";
    do{
        this->semaphore_Server = sem_open(sem_server.c_str(), 0);
    } while (this->semaphore_Server==SEM_FAILED);
	//std::cout<<"after get sema 2\n";
    do{
        this->semaphore_GetBack = sem_open(sem_getback.c_str(), 0);
    } while (this->semaphore_GetBack==SEM_FAILED);
	//std::cout<<"after get sema 3\n";


    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    strcpy(message->shmId, appName.c_str());
    return;
}

int CachelibClient::addpool(string poolName)
{
    cout<<"------shared memory------\n";
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
	string message = "A:" + poolName;
	//std::cout<<"send message is: "<<message<<std::endl;
	int bytesSent = send(client_socket, message.c_str(), message.size() + 1, 0);
	if (bytesSent == -1){
		cout<<"Error: Failed to send message\n";
		close(client_socket);
		exit(EXIT_FAILURE);
	}

	char buffer[32];
	memset(buffer, 0, sizeof(buffer));
	int bytesReceived = recv(client_socket, buffer, 32, 0);
	if (bytesReceived == -1){
		cout<<"Error: Failed to recv response\n";
		close(client_socket);
		exit(EXIT_FAILURE);
	}
	close(client_socket);
	string recvInfo = buffer;
	//std::cout<<"client recv is: "<<recvInfo<<std::endl;
	size_t spacePosition = recvInfo.find(' ');
	this->pid = stoi(recvInfo.substr(0, spacePosition));
	this->shmId = recvInfo.substr(spacePosition+1);

	this->prefix = poolName + "_";
	prepare_shm(this->shmId);
	return this->pid;
}

void CachelibClient::setKV(string key,string value)
{
    //锁资源
    while(sem_trywait(this->semaphore_Server)!=0);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=SIG_SET;
    message->pid=this->pid;

    memset(message->key,0,sizeof(message->key));
    memset(message->value,0,sizeof(message->value));
    strcpy(message->key,(this->prefix+key).c_str());
    strcpy(message->value,value.c_str());

    //释放资源
    sem_post(this->semaphore);
    return ;
}

string CachelibClient::getKV(string key)
{
    //锁资源
    while(sem_wait(this->semaphore_Server)!=0);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=SIG_GET;
    memset(message->key,0,sizeof(message->key));
    memset(message->value,0,sizeof(message->value));

    strcpy(message->key,(this->prefix+key).c_str());
    sem_post(this->semaphore);

    //等待回传
    while(sem_trywait(this->semaphore_GetBack)!=0);
    memset(this->getValue,0,sizeof(this->getValue));


    //control cache miss
    double randomNum = dis(gen);
    if(randomNum>=0){
    	strcpy(this->getValue,message->value);

    	//释放资源
    	sem_post(this->semaphore_Server);
    	if(strlen(this->getValue)!=0)
        	this->getHit++;
    	//cout<<"in clientAPI get value is: "<<this->getValue<<endl;
    	return this->getValue;
    }else{
	    sem_post(this->semaphore_Server);
	    return this->getValue;
    }
}

bool CachelibClient::delKV(string key)
{
    //锁资源
    while(sem_trywait(this->semaphore_Server)!=0);
    //sem_wait(this->semaphore_Server);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=2;
    memset(message->key,0,sizeof(message->key));
    memset(message->value,0,sizeof(message->value));
    strcpy(message->key,(this->prefix+key).c_str());
    //释放资源
    sem_post(this->semaphore);
    return true;
}
