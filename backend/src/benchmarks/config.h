#pragma once

#define DISTRIBUTION_ZIPFIAN 1
#define DISTRIBUTION_UNIFORM 2
#define DISTRIBUTION_SEQUENTIAL 3
#define DISTRIBUTION_HOTSPOT 4
#define DISTRIBUTION_EXPONENTIAL 5

#define UNIFIED_CACHE_POOL "tmdb_sequential_1K"
#define BACKEND TmdbBackend
// #define BACKEND MySQLBackend
// #define BACKEND MongoDBBackend
// #define BACKEND LevelDBBackend
// #define BACKEND SQLiteBackend

#define REMOTE_CACHE 0
#define MAX_RECORDS 1048576 // number of records in the database
#define MAX_FIELDS 1 // number of fields in each record
#define MAX_FIELD_SIZE 1000 // size of each field (in chars)
#define MAX_QUERIES 100000 // number of queries to execute
#define QUERY_PROPORTION 1 // proportion of read queries, the rest are insert queries

#define DISTRIBUTION DISTRIBUTION_HOTSPOT

#if DISTRIBUTION == DISTRIBUTION_ZIPFIAN
    #define ZIPFIAN_SKEW 0.99
#elif DISTRIBUTION == DISTRIBUTION_HOTSPOT
    // p(x \in [0, HOTSPOT_PROPORTION * N)) = HOTSPOT_ALPHA
    // p(x \in [HOTSPOT_PROPORTION * N, N)) = (1 - HOTSPOT_ALPHA)
    #define HOTSPOT_PROPORTION 0.2
    #define HOTSPOT_ALPHA 0.8
#elif DISTRIBUTION == DISTRIBUTION_EXPONENTIAL
    #define EXPONENTIAL_LAMBDA 0.1
#endif
