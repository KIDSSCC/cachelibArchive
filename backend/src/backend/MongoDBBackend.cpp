#include "MongoDBBackend.h"
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/exception.hpp>
#include "utils/cache_utils.h"

static mongocxx::instance instance{};

MongoDBBackend::MongoDBBackend(int thread_id) : Backend(thread_id) {
    std::lock_guard<std::mutex> lock(mtx);
    client = mongocxx::client{mongocxx::uri{MONGODB_URI}};
    db_name = "ycsb"; // Default database for YCSB benchmarks
    db = client[db_name];
    collection_name = COLLECTION_PREFIX + std::to_string(thread_id);
    collection = db[collection_name];
}

MongoDBBackend::~MongoDBBackend() {
    // cleanup if necessary
}

bool MongoDBBackend::create_database() {
    try {
        collection.drop(); // Ensure a clean state if exists
        bsoncxx::builder::basic::document index_builder{};
        index_builder.append(bsoncxx::builder::basic::kvp("ycsb_key", 1));
        collection.create_index(index_builder.view(), mongocxx::options::index{}.unique(true));
        return true;
    } catch (const mongocxx::exception& e) {
        std::cerr << "MongoDB create database error: " << e.what() << std::endl;
        return false;
    }
}

bool MongoDBBackend::read_record(int key, std::vector<std::string>& results) {
    if (cache_enabled) {
        std::string json = cache.get_(std::to_string(key));
        if (json != "") {
            auto view = bsoncxx::from_json(json);
            for (int i = 1; i <= MAX_FIELDS; i++) {
                std::string field_name = "field" + std::to_string(i);
                if (view[field_name] && view[field_name].type() == bsoncxx::type::k_utf8) {
                    results.push_back(view[field_name].get_string().value.to_string());
                }
            }
            hit_count++;
            return true;
        }
    }

    try {
        bsoncxx::builder::basic::document filter_builder{};
        filter_builder.append(bsoncxx::builder::basic::kvp("ycsb_key", key));

        auto doc = collection.find_one(filter_builder.view());
        if (doc) {
            auto view = doc->view();
            for (int i = 1; i <= MAX_FIELDS; i++) {
                std::string field_name = "field" + std::to_string(i);
                if (view[field_name] && view[field_name].type() == bsoncxx::type::k_utf8) {
                    results.push_back(view[field_name].get_string().value.to_string());
                }
            }
            if (cache_enabled) {
                if (!cache.set_(std::to_string(key), bsoncxx::to_json(view))) {
                    std::cout << "MongoDBBackend: cache set failed" << std::endl;
                }
            }
            return true;
        }
        return false;
    } catch (const mongocxx::exception& e) {
        std::cerr << "MongoDB read record error: " << e.what() << std::endl;
        return false;
    }
}

bool MongoDBBackend::insert_record(int key, std::vector<std::string>& values) {
    try {
        bsoncxx::builder::basic::document document_builder{};
        document_builder.append(bsoncxx::builder::basic::kvp("ycsb_key", key));

        for (size_t i = 0; i < values.size(); i++) {
            document_builder.append(bsoncxx::builder::basic::kvp("field" + std::to_string(i+1), values[i]));
        }
        collection.insert_one(document_builder.view());

        if (cache_enabled) {
            if (!cache.set_(std::to_string(key), bsoncxx::to_json(document_builder.view()))) {
                std::cout << "MongoDBBackend: cache set failed" << std::endl;
            }
        }

        return true;
    } catch (const mongocxx::exception& e) {
        std::cerr << "MongoDB insert record error: " << e.what() << std::endl;
        return false;
    }
}

bool MongoDBBackend::clean_up() {
    // delete newly inserted records >= MAX_RECORDS
    try {
        bsoncxx::builder::basic::document filter_builder{};
        filter_builder.append(bsoncxx::builder
            ::basic::kvp("ycsb_key", bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$gte", MAX_RECORDS)))
        );
        collection.delete_many(filter_builder.view());
        return true;
    } catch (const mongocxx::exception& e) {
        std::cerr << "MongoDB clean up error: " << e.what() << std::endl;
        return false;
    }
}