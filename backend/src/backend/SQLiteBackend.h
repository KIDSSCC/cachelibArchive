#pragma once

#include "common.h"
#include "backend.h"
#include "utils/randstring.h"
#include <iostream>
#include <mutex>
#include <sqlite3.h>

#define SQLITE_DIR "./sqlite"
#define SQLITE_TABLE_PREFIX "usertable"

class SQLiteBackend : public Backend {
public:
    SQLiteBackend(int thread_id);
    ~SQLiteBackend();
    bool create_database();
    bool read_record(int key, std::vector<std::string>& results);
    bool insert_record(int key, std::vector<std::string>& values);
    bool clean_up();

private:
    std::string db_path = SQLITE_DIR "/db.db";
    sqlite3* db;
    sqlite3_stmt* insert_stmt;
    std::string table_name = SQLITE_TABLE_PREFIX "_default";

    static std::mutex mtx;
    bool execute_update(const std::string& statement);
    sqlite3_stmt* execute_query(const std::string& statement);

    std::string generate_read_sql(int key);
    std::string generate_insert_sql(int key, const std::vector<std::string>& values);
};
