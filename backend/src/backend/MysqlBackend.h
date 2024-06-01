#pragma once

#include "common.h"
#include "backend.h"
#include "utils/randstring.h"
#include <iostream>
#include <mutex>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#define MYSQL_HOST "tcp://127.0.0.1:3306"
#define MYSQL_USERNAME "root"
#define MYSQL_PASSWORD "zheshimima"
#define MYSQL_DATABASE "ycsb"
#define MYSQL_TABLE_PREFIX "usertable"

class MySQLBackend : public Backend {
public:
    MySQLBackend(int thread_id);
    ~MySQLBackend();
    bool create_database();
    bool read_record(int key, std::vector<std::string>& results);
    bool insert_record(int key, std::vector<std::string>& values);
    bool clean_up();

private:
    sql::mysql::MySQL_Driver* driver;
    std::unique_ptr<sql::Connection> conn;
    std::unique_ptr<sql::Statement> stmt;
    std::string table_name = MYSQL_TABLE_PREFIX "_default";

    static std::mutex mtx;
    bool execute_update(std::string statement);
    std::unique_ptr<sql::ResultSet> execute_query(std::string statement);

    std::string generate_read_sql(int key);
    std::string generate_insert_sql(int key, std::vector<std::string>& values);
};