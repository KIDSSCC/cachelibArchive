#pragma once

#include "common.h"
#include "backend.h"
#include "utils/randstring.h"
#include <iostream>
#include "tmdb.h"

#define TMDB_TABLE_PREFIX "tmdb_1"

class TmdbBackend : public Backend{
public:
    TmdbBackend(int thread_id);
    ~TmdbBackend();
    bool create_database();
    bool read_record(int key, std::vector<std::string>& results);
    bool insert_record(int key, std::vector<std::string>& values);
    bool clean_up();

private:
    string disk_name;
    TDB* db;
}