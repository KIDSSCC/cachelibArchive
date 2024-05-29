#pragma once

#include "backend.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <vector>
#include <string>
#include <mutex>

#define MONGODB_URI "mongodb://localhost:27017"
#define COLLECTION_PREFIX "usertable"

class MongoDBBackend : public Backend {
private:
    mongocxx::client client;
    mongocxx::database db;
    std::string db_name;
    std::string collection_name = COLLECTION_PREFIX "_default";
    std::mutex mtx;
    mongocxx::v_noabi::collection collection;

public:
    MongoDBBackend(int thread_id);
    ~MongoDBBackend() override;
    bool create_database() override;
    bool read_record(int key, std::vector<std::string>& results) override;
    bool insert_record(int key, std::vector<std::string>& values) override;
    bool clean_up() override;
};
