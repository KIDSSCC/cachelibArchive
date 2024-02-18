#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <thread>

#include "cachelibHeader.h"
#include "messageInfo.h"


using namespace std;
using namespace facebook::cachelib_examples;

int msgid;
void listen_addpool()
{
    while(1)
    {
        m_addpool_c rcv;
        if(msgrcv(msgid, &rcv, sizeof(m_addpool_c)-sizeof(long),ADDPOOL_C,0)==-1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        rcv.pid=addpool_(rcv.name);
        rcv.mtype = ADDPOOL_S;
        if(msgsnd(msgid, &rcv, sizeof(m_addpool_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
    }
}
void listen_set()
{
    while(1)
    {
        m_set_c rcv;
        if(msgrcv(msgid, &rcv, sizeof(m_set_c)-sizeof(long),SETKV_C,0)==-1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        rcv.res=set_(rcv.pid,rcv.key,rcv.value);
        /*
        rcv.mtype = SETKV_S;
        if(msgsnd(msgid, &rcv, sizeof(m_set_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        */
    }
}
void listen_get()
{
    while(1)
    {
        m_get_c rcv;
        if(msgrcv(msgid, &rcv, sizeof(m_get_c)-sizeof(long),GETKV_C,0)==-1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        string value=get_(rcv.key);
        if(value=="")
        {
            rcv.res=false;
        }
        else
        {
            rcv.res=true;
            strcpy(rcv.value,value.c_str());
        }
        rcv.mtype = GETKV_S;
        if(msgsnd(msgid, &rcv, sizeof(m_get_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
    }
}
void listen_del()
{
    while(1)
    {
        m_del_c rcv;
        if(msgrcv(msgid, &rcv, sizeof(m_del_c)-sizeof(long),DEL_C,0)==-1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        rcv.res=del_(rcv.key);
        rcv.mtype=DEL_S;
        if(msgsnd(msgid, &rcv, sizeof(m_del_c)-sizeof(long), 0) == -1) 
        {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
    }
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
    
    //Service
    MQInit();
    thread t1(listen_addpool);
    thread t3(listen_set);
    thread t2(listen_get);
    thread t4(listen_del);
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    destroyCache();
}