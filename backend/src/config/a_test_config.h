#pragma once

#define UNIFIED_CACHE_POOL "test_name_A"

#define DISTRIBUTION_ZIPFIAN 1
#define DISTRIBUTION_UNIFORM 2
#define DISTRIBUTION_SEQUENTIAL 3

// kind of backend
#define BACKEND TmdbBackend
// #define BACKEND MySQLBackend
// #define BACKEND MongoDBBackend
// #define BACKEND LevelDBBackend
// #define BACKEND SQLiteBackend

#define MAX_RECORDS 1000 // number of records in the database
#define MAX_FIELDS 1 // number of fields in each record
#define MAX_FIELD_SIZE 100 // size of each field (in chars)
#define MAX_QUERIES 20000 // number of queries to execute
#define QUERY_PROPORTION 1 // proportion of read queries, the rest are insert queries
#define OUTPUT std::cout

#ifdef DISTRIBUTION_ZIPFIAN
    #define ZIPFIAN_SKEW 0.99
#endif

#ifdef DISTRIBUTION_HOTSPOT
    #define HOTSPOT_PROPORTION 0.1
    #define HOTSPOT_ALPHA 0.8
#endif

#ifdef DISTRIBUTION_EXPONENTIAL
    #define EXPONENTIAL_LAMBDA 0.1
#endif