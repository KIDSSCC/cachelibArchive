#ifndef CS_CONFIG
#define CS_CONFIG

//for util
#define GB_SIZE ((size_t)1024 * 1024 * 1024)
#define MB_SIZE ((size_t)1024 * 1024)
#define KB_SIZE ((size_t)1024)

// for cachelib header
#define CACHE_SIZE 30 * GB_SIZE
#define REDUNDARY_SIZE (size_t)4 * 1024 * 1024

#define POOL_SIZE 16 * MB_SIZE

#define HYBRID_CACHE 0
#define CREATE_DEFAULT_POOL 0

// for server
#define MAX_WAIT 1e9
#define SIZE_CONV ((size_t)64 * 1024 * 1024)   //Unit of cache resize
#define CACHE_HIT 1

// for dcp
#define DCP_PORT 1412

#include<vector>

#define OTHER_NODE                          \
        "127.0.0.1:7890"         \
        "127.0.0.1:7891",           

#endif
