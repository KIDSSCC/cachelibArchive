#ifndef CS_CONFIG
#define CS_CONFIG

// for cachelib header
#define CACHE_SIZE (size_t)10 * 1024 * 1024 * 1024
#define REDUNDARY_SIZE (size_t)4 * 1024 * 1024

#define POOL_SIZE (size_t)2 * 1024 * 1024 * 1024
#define DEFAULT_POOL_SIZE (size_t)10 * 1024 * 1024 * 1024

#define HYBRID_CACHE 0
#define CREATE_DEFAULT_POOL 0

// for server
#define MAX_WAIT 100000000 
#define SIZE_CONV ((size_t)256 * 1024 * 1024)   //Unit of cache resize

#define CACHE_HIT 1

#endif
