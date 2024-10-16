#pragma once

#define UNIFIED_CACHE_POOL "test_name_A"

// Generator{distribution, {skew, ...}, workingset_size, querys}
// Backend {TmdbBackend, MySQLBackend, MongoDBBackend, LevelDBBackend, SQLiteBackend}
// enum Distribution{
//     D_ZIPFIAN,
//     D_HOTSPOT,
//     D_UNIFORM,
//     D_EXPONENTIAL,
//     D_SEQUENTIAL
// };
#define WORKLOAD_TYPE {                         \
        {D_ZIPFIAN, {0.99}, 500, 2000},         \
        {D_HOTSPOT, {0.2, 0.8}, 500, 2000},     \
        {D_SEQUENTIAL, {}, 1000, 2000},     \
    }


// kind of backend
#define BACKEND TmdbBackend

#define MAX_RECORDS 1000 // number of records in the database
#define MAX_FIELDS 1 // number of fields in each record
#define MAX_FIELD_SIZE 100 // size of each field (in chars)
#define QUERY_PROPORTION 1 // proportion of read queries, the rest are insert queries
#define OUTPUT std::cout