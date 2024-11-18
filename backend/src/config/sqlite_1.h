#pragma once
#include "generator/generator.h"
#include <memory>
using namespace std;

#define UNIFIED_CACHE_POOL "sqlite_1"

// Generator{distribution, {skew, ...}, workingset_size, querys}
// Backend {TmdbBackend, MySQLBackend, MongoDBBackend, LevelDBBackend, SQLiteBackend}
// enum Distribution{
//     D_ZIPFIAN,
//     D_HOTSPOT,
//     D_UNIFORM,
//     D_EXPONENTIAL,
//     D_SEQUENTIAL
// };

#define WORKLOAD_TYPE                          \
        std::make_shared<Generator>(D_ZIPFIAN, 420000, vector<double>{0.9}),         \
        std::make_shared<Generator>(D_SEQUENTIAL, 1620000, vector<double>{}),         \
        std::make_shared<Generator>(D_UNIFORM, 315000, vector<double>{}),     
    


// kind of backend
#define BACKEND SQLiteBackend

#define MAX_RECORDS 1620000 // number of records in the database
#define MAX_FIELDS 1 // number of fields in each record
#define MAX_FIELD_SIZE 10000 // size of each field (in chars)
#define MAX_QUERIES 20000
#define QUERY_PROPORTION 1 // proportion of read queries, the rest are insert queries
#define OUTPUT std::cout