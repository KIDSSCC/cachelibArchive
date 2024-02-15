#include "clientAPI.h"

CachelibClient::CachelibClient()
{
    this->msgid=msgget(MSG_KEY, 0666);
    if (this->msgid == -1) 
    {
        perror("msgget");
        cerr<<"MQ not exist,Server is offline\n";
        exit(EXIT_FAILURE);
    }
    this->getHit=0;
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
    cout<<"get pool pid is: "<<snd.pid<<endl;
    this->pid=snd.pid;
    this->prefix = to_string(pid) + "_";
    return pid;
}

bool CachelibClient::setKV(string key,string value)
{
    m_set_c snd;
    snd.mtype=SETKV_C;
    snd.pid=this->pid;
    //增加前缀
    strcpy(snd.key,(this->prefix+key).c_str());
    strcpy(snd.value,value.c_str());
    
    if(msgsnd(this->msgid, &snd, sizeof(m_set_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    if(msgrcv(this->msgid,&snd,sizeof(m_set_c)-sizeof(long),SETKV_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    //cout<<"set operation------key is: "<<key<<" value is: "<<value<<endl;
    return snd.res;
}

string CachelibClient::getKV(string key)
{
    m_get_c snd;
    snd.mtype=GETKV_C;
    strcpy(snd.key,(this->prefix+key).c_str());
    if(msgsnd(this->msgid, &snd, sizeof(m_get_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    if(msgrcv(this->msgid,&snd,sizeof(m_get_c)-sizeof(long),GETKV_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    /*
    if(snd.res)
    {
        cout<<"get operation------key is: "<<key<<" value is: "<<snd.value<<endl;
    }
    else
    {
        cout<<"get no info\n";
    }
    */
    if(snd.res)
    {
        this->getHit++;
        return snd.value;
    }
    else
        return "";
}

bool CachelibClient::delKV(string key)
{
    m_del_c snd;
    snd.mtype=DEL_C;
    strcpy(snd.key,(this->prefix+key).c_str());
    if(msgsnd(msgid, &snd, sizeof(m_del_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    if(msgrcv(msgid,&snd,sizeof(m_del_c)-sizeof(long),DEL_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    return snd.res;
}

void set_util(int msgid,m_set_c* snd)
{
    if(msgsnd(msgid, snd, sizeof(m_set_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    if(msgrcv(msgid,snd,sizeof(m_set_c)-sizeof(long),SETKV_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
}

void CachelibClient::setKV_async(string key,string value)
{
    m_set_c snd;
    snd.mtype=SETKV_C;
    snd.pid=this->pid;
    //增加前缀
    strcpy(snd.key,(this->prefix+key).c_str());
    strcpy(snd.value,value.c_str());
    future<void> setoper=async(launch::async,set_util,this->msgid,&snd);
}

/*
void CachelibClient::delKV_async(string key)
{
    future<void> deloper=async(launch::async,this->delKV,key);
}
*/