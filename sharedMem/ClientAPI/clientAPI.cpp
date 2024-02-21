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

void CachelibClient::prepare_shm(string appName)
{
    int SHARED_MEMORY_SIZE = sizeof(shm_stru);
    string sem_server=string(appName)+"_Server";
    string sem_getback=string(appName)+"_getback";
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
    do
    {
        this->semaphore = sem_open(appName.c_str(), 0);
    } while (this->semaphore==SEM_FAILED);
    do
    {
        this->semaphore_Server = sem_open(sem_server.c_str(), 0);
    } while (this->semaphore_Server==SEM_FAILED);
    do
    {
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
    if(msgrcv(this->msgid,&snd,sizeof(m_addpool_c)-sizeof(long),ADDPOOL_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    this->pid=snd.pid;
    this->prefix = to_string(pid) + "_";

    prepare_shm(poolName);
    return pid;
}

bool CachelibClient::setKV(string key,string value)
{
    thread t(&CachelibClient::setKV_util,this,key,value);
    t.detach();
    return true;
}

char* CachelibClient::getKV(string key)
{
    //锁资源
    sem_wait(this->semaphore_Server);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=1;
    memset(message->key,0,sizeof(message->key));
    memset(message->value,0,sizeof(message->value));

    strcpy(message->key,(this->prefix+key).c_str());
    sem_post(this->semaphore);

    //等待回传
    sem_wait(this->semaphore_GetBack);
    string value=message->value;

    //释放资源
    sem_post(this->semaphore_Server);
    if(value!="")
        this->getHit++;
    return message->value;
}

bool CachelibClient::delKV(string key)
{
    //锁资源
    sem_wait(this->semaphore_Server);
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

void CachelibClient::setKV_util(string key,string value)
{
    //锁资源
    sem_wait(this->semaphore_Server);
    //准备要存入共享内存的数据
    shm_stru* message=static_cast<shm_stru*>(this->shared_memory);
    message->ctrl=0;
    message->pid=this->pid;

    memset(message->key,0,sizeof(message->key));
    memset(message->value,0,sizeof(message->value));

    strcpy(message->key,(this->prefix+key).c_str());
    strcpy(message->value,value.c_str());

    //释放资源
    sem_post(this->semaphore);
}