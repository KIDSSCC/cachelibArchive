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

    this->set_snd=(m_set_c*)malloc(sizeof(struct m_set_c));
    this->get_snd=(m_get_c*)malloc(sizeof(struct m_get_c));

    

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

    this->set_snd->pid=this->pid;
    return pid;
}

bool CachelibClient::setKV(string key,string value)
{
    memset(set_snd->key,0,sizeof(set_snd->key));
    memset(set_snd->value,0,sizeof(set_snd->value));

    set_snd->mtype=SETKV_C;
    strcpy(set_snd->key,(this->prefix+key).c_str());
    strcpy(set_snd->value,value.c_str());

    if(msgsnd(this->msgid, set_snd, sizeof(m_set_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    /*
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
    */

    /*
    if(msgrcv(this->msgid,&snd,sizeof(m_set_c)-sizeof(long),SETKV_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    */
    //cout<<"set operation------key is: "<<key<<" value is: "<<value<<endl;
    return true;
}

string CachelibClient::getKV(string key)
{
    memset(get_snd->key,0,sizeof(get_snd->key));
    memset(get_snd->value,0,sizeof(get_snd->value));

    get_snd->mtype=GETKV_C;
    strcpy(get_snd->key,(this->prefix+key).c_str());
    get_snd->res=false;
    if(msgsnd(this->msgid, get_snd, sizeof(m_get_c)-sizeof(long),0)==-1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    if(msgrcv(this->msgid,get_snd,sizeof(m_get_c)-sizeof(long),GETKV_S,0)==-1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    if(get_snd->res)
    {
        this->getHit++;
        return get_snd->value;
    }
    else
        return "";
    /*
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
    
    if(snd.res)
    {
        this->getHit++;
        return snd.value;
    }
    else
        return "";
    */
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