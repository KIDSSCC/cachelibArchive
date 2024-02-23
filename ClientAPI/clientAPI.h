#ifndef CLIENT_API
#define CLIENT_API

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <future>

#include "messageInfo.h"


using namespace std;
class CachelibClient
{
private:
    int pid;
    int msgid;
    string prefix;

    m_set_c* set_snd;
    m_get_c* get_snd;
public:
    int getHit;
    CachelibClient();
    int addpool(string poolName);
    bool setKV(string key,string value);
    char* getKV(string key);
    bool delKV(string key);

    void setKV_async(string key,string value);
    //void delKV_async(string key);
};


#endif