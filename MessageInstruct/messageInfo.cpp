#include "messageInfo.h"

struct m_addpool_c
{
    long mtype;
    char name[1024];
    int pid;
};