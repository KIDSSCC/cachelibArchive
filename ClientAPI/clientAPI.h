#ifndef CLIENT_API
#define CLIENT_API

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "messageInfo.h"


using namespace std;
class CachelibClient
{
private:
    int pid;
    int msgid;
    string prefix;
public:
    int getHit;
    CachelibClient();
    int addpool(string poolName);
    bool setKV(string key,string value);
    string getKV(string key);
    bool delKV(string key);
};


#endif