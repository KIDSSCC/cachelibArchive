#ifndef MESSAHE_INFO
#define MESSAHE_INFO

#define MSG_KEY 1412
#define MSG_KEY_SIZE 512
#define MSG_VALUE_SIZE 1024

#define ADDPOOL_C 1
#define ADDPOOL_S 11
#define SETKV_C 2
#define SETKV_S 12
#define GETKV_C 3
#define GETKV_S 13
#define DEL_C 4
#define DEL_S 14

struct m_addpool_c
{
    long mtype;
    char name[MSG_KEY_SIZE];
    int pid;
};

struct m_set_c
{
    long mtype;
    int pid;
    char key[MSG_KEY_SIZE];
    char value[MSG_VALUE_SIZE];
    bool res;
};

struct m_get_c
{
    long mtype;
    char key[MSG_KEY_SIZE];
    char value[MSG_VALUE_SIZE];
    bool res;
};

struct m_del_c
{
    long mtype;
    char key[MSG_KEY_SIZE];
    bool res;
};


#endif