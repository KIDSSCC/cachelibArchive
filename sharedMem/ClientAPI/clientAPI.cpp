#include "clientAPI.h"


CachelibClient::CachelibClient()
{
    cout<<"------shared memory------\n";
    this->msgid=msgget(MSG_KEY, 0666);
    if (this->msgid == -1) 
    {
        perror("msgget");
        cerr<<"MQ not exist,Server is offline\n";
        exit(EXIT_FAILURE);
    }
    this->getHit=0;
}
/*
CachelibClient::CachelibClient()
{
    cout<<"------shared memory------\n";
    char tmpaddr[]="tmp_add_pool_shm";
    int SHARED_MEMORY_SIZE = sizeof(app_name);
    int addpool_shm_fd;
    //打开共享内存
    do{
        addpool_shm_fd = shm_open(tmpaddr, O_RDWR, 0666);
    } while (addpool_shm_fd == -1);
    // 将共享内存映射到进程的地址空间
    
    this->addpool_shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, addpool_shm_fd, 0);
    if (this->addpool_shared_memory == MAP_FAILED) 
    {
        perror("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }
    //打开信号量
    string sema_client_name=string(tmpaddr)+"_client";
    string sema_write_right_name=string(tmpaddr)+"_write";
    string sema_add_pool_back_name=string(tmpaddr)+"_back";
    do{
        this->sema_client = sem_open(sema_client_name.c_str(), 0);
    } while (this->sema_client==SEM_FAILED);
    do{
        this->sema_write_right=sem_open(sema_write_right_name.c_str(),0);
    }while (this->sema_write_right==SEM_FAILED);
    do{
        this->sema_add_pool_back=sem_open(sema_add_pool_back_name.c_str(),0);
    }while (this->sema_add_pool_back==SEM_FAILED);
}*/

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
    string sem_server=string(appName)+"_Server";
    string sem_getback=string(appName)+"_getback";
    do{
        this->semaphore = sem_open(appName.c_str(), 0);
    } while (this->semaphore==SEM_FAILED);
    do{
        this->semaphore_Server = sem_open(sem_server.c_str(), 0);
    } while (this->semaphore_Server==SEM_FAILED);
    do{
        this->semaphore_GetBack = sem_open(sem_getback.c_str(), 0);
    } while (this->semaphore_GetBack==SEM_FAILED);
}

int CachelibClient::addpool(string poolName)
{
    m_addpool_c snd;
    snd.mtype=ADDPOOL_C;
    strcpy(snd.name,poolName.c_str());
    if(msgsnd(this->msgid, &snd, sizeof(m_addpool_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    
    //非阻塞轮询的方式等待回传
    while(msgrcv(this->msgid,&snd,sizeof(m_addpool_c)-sizeof(long),ADDPOOL_S,IPC_NOWAIT)==-1);
    this->pid=snd.pid;
    //this->prefix = to_string(pid) + "_";
    this->prefix = poolName + "_";
    
    prepare_shm(poolName);
    return pid;
}
/*
int CachelibClient::addpool(string poolName)
{
    //锁资源
    while(sem_trywait(this->sema_write_right)!=0);
    app_name* message=static_cast<app_name*>(this->addpool_shared_memory);
    strcpy(message->name,poolName.c_str());
    //通知服务端处理
    sem_post(this->sema_client);
    //等待回传
    while(sem_trywait(this->sema_add_pool_back)!=0);
    this->pid=message->pid;
    this->prefix = to_string(this->pid) + "_";
    cout<<"get pid is: "<<this->pid<<endl;
    //释放资源
    sem_post(this->sema_write_right);
    //准备其他的信号量
    prepare_shm(poolName);
    return this->pid;
}*/

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

char* CachelibClient::getKV(string key)
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
    strcpy(this->getValue,message->value);

    //释放资源
    sem_post(this->semaphore_Server);
    if(strlen(this->getValue)!=0)
        this->getHit++;
    //cout<<"in clientAPI get value is: "<<this->getValue<<endl;
    return this->getValue;
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
