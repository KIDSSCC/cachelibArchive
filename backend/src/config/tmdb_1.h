#pragma once
#include "generator/generator.h"
#include <memory>
using namespace std;

#define UNIFIED_CACHE_POOL "tmdb_1"

/*
definition of Generator
        Generator{distribution, workingset_size, vector<double>{params}}
enum Distribution{
     D_ZIPFIAN,
     D_HOTSPOT,
     D_UNIFORM,
     D_EXPONENTIAL,
     D_SEQUENTIAL
 };

Backend {TmdbBackend, MySQLBackend, MongoDBBackend, LevelDBBackend, SQLiteBackend}
*/

// kind of backend
#define BACKEND TmdbBackend

#define MAX_RECORDS 1048576 // number of records in the database
#define MAX_FIELDS 1 // number of fields in each record
#define MAX_FIELD_SIZE 10000 // size of each field (in chars)
#define MAX_QUERIES 100000
#define QUERY_PROPORTION 1 // proportion of read queries, the rest are insert queries
#define OUTPUT std::cout

#define WORKLOAD_TYPE                          \
        std::make_shared<Generator>(D_UNIFORM, 1048576, vector<double>{}),
