#include "SQLiteBackend.h"
#include "utils/cache_utils.h"

std::mutex SQLiteBackend::mtx;

SQLiteBackend::SQLiteBackend(int thread_id) : Backend(thread_id) {
    // check if dir SQLITE_DIR exists
    std::string dir = SQLITE_DIR;
    struct stat info;
    if (stat(dir.c_str(), &info) != 0) {
        std::string command = "mkdir " + dir;
        if (system(command.c_str()) != 0) {
            throw std::runtime_error("Failed to create directory " + dir);
        }
    }
    // check if multi-threading is supported
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    if (sqlite3_threadsafe() == 0) {
        throw std::runtime_error("SQLite is not compiled with multi-threading support");
    }

    db_path = SQLITE_DIR "/db" + std::to_string(thread_id) + ".db";
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (sqlite3_open(db_path.c_str(), &db)) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            db = nullptr;
        }
        sqlite3_busy_timeout(db, 10000); // it should be enough for most cases
    }
    table_name = SQLITE_TABLE_PREFIX + std::to_string(thread_id);
}

SQLiteBackend::~SQLiteBackend() {
    if (db) {
        sqlite3_close(db);
    }
}

bool SQLiteBackend::create_database() {
    std::string drop_old_table_sql = "DROP TABLE IF EXISTS " + table_name + ";";
    execute_update(drop_old_table_sql);

    std::string create_table_sql = "CREATE TABLE IF NOT EXISTS " + table_name + " (ycsb_key INTEGER PRIMARY KEY, field1 TEXT, field2 TEXT, field3 TEXT, field4 TEXT, field5 TEXT, field6 TEXT, field7 TEXT, field8 TEXT, field9 TEXT, field10 TEXT);";
    execute_update(create_table_sql);

    std::string insert_sql = "INSERT INTO " + table_name + " (ycsb_key, field1, field2, field3, field4, field5, field6, field7, field8, field9, field10) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, nullptr);

    return true;
}

bool SQLiteBackend::read_record(int key, std::vector<std::string>& results) {
    std::string sql = generate_read_sql(key);
    if (cache_enabled) {
        std::string value = cache.get_(sql);
        if (value != "") {
            results = unpack_values(value, MAX_FIELD_SIZE);
            hit_count++;
            return true;
        }
    }
    sqlite3_stmt* stmt = execute_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        for (int i = 0; i < 10; i++) {
            results.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i + 1)));
        }
        sqlite3_finalize(stmt);

        if (cache_enabled) {
            if (!cache.set_(sql, pack_values(results))) {
                std::cout << "SQLiteBackend: cache set failed" << std::endl;
            }
        }
        return true;
    }
    sqlite3_finalize(stmt);
    return false;
}

bool SQLiteBackend::insert_record(int key, std::vector<std::string>& values) {
    std::string sql = generate_insert_sql(key, values);
    return execute_update(sql);
}

bool SQLiteBackend::execute_update(const std::string& statement) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, statement.c_str(), nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}

sqlite3_stmt* SQLiteBackend::execute_query(const std::string& statement) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, statement.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to execute query: " << statement << std::endl;
        return nullptr;
    }
    return stmt;
}

std::string SQLiteBackend::generate_read_sql(int key) {
    return "SELECT * FROM " + table_name + " WHERE ycsb_key = " + std::to_string(key) + ";";
}

std::string SQLiteBackend::generate_insert_sql(int key, const std::vector<std::string>& values) {
    std::string sql = "INSERT INTO " + table_name + " (ycsb_key, field1, field2, field3, field4, field5, field6, field7, field8, field9, field10) VALUES (" + std::to_string(key);
    for (const auto& value : values) {
        sql += ", '" + value + "'";
    }
    sql += ")";
    return sql;
}

bool SQLiteBackend::clean_up() {
    execute_update("DELETE FROM " + table_name + " WHERE ycsb_key >= " + std::to_string(MAX_RECORDS));
    return true;
}
