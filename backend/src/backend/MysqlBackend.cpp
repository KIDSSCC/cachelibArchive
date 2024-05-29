#include "MysqlBackend.h"
#include "utils/cache_utils.h"

std::mutex MySQLBackend::mtx;

MySQLBackend::MySQLBackend(int thread_id) : Backend(thread_id) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        driver = sql::mysql::get_mysql_driver_instance();
    }
    conn = std::unique_ptr<sql::Connection>(driver->connect(
        MYSQL_HOST, MYSQL_USERNAME, MYSQL_PASSWORD
    ));
    conn->setSchema(MYSQL_DATABASE);
    stmt = std::unique_ptr<sql::Statement>(conn->createStatement());
    table_name = MYSQL_TABLE_PREFIX + std::to_string(thread_id);
}

MySQLBackend::~MySQLBackend() {
    conn->close();
}

bool MySQLBackend::create_database() {
    /*
    SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;
    SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;
    DROP TABLE IF EXISTS usertable;
    CREATE TABLE usertable (
        ycsb_key int PRIMARY KEY,
        field1   varchar(100),
        field2   varchar(100),
        field3   varchar(100),
        field4   varchar(100),
        field5   varchar(100),
        field6   varchar(100),
        field7   varchar(100),
        field8   varchar(100),
        field9   varchar(100),
        field10  varchar(100)
    );
    SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
    SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;
    */
    stmt->execute("SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;");
    stmt->execute("SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;");
    stmt->execute("DROP TABLE IF EXISTS " + table_name + ";");
    stmt->execute("CREATE TABLE " + table_name + " (ycsb_key int PRIMARY KEY, field1 varchar(100), field2 varchar(100), field3 varchar(100), field4 varchar(100), field5 varchar(100), field6 varchar(100), field7 varchar(100), field8 varchar(100), field9 varchar(100), field10 varchar(100));");
    stmt->execute("SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;");
    stmt->execute("SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;");
    insert_stmt = std::unique_ptr<sql::PreparedStatement>(
            conn->prepareStatement("INSERT INTO " + table_name + " (ycsb_key, field1, field2, field3, field4, field5, field6, field7, field8, field9, field10) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
        );
    return true;
}

bool MySQLBackend::read_record(int key, std::vector<std::string>& results) {
    std::string sql = generate_read_sql(key);
    if (cache_enabled) {
        std::string value = cache.get_(sql);
        if (value != "") {
            results = unpack_values(value, MAX_FIELD_SIZE);
            hit_count++;
            return true;
        }
    }
    std::unique_ptr<sql::ResultSet> res = execute_query(sql);
    if (res->next()) {
        for (int i = 1; i <= 10; i++) {
            results.push_back(res->getString(i));
        }
        if (cache_enabled) {
            if (!cache.set_(sql, pack_values(results))) {
                std::cout << "MySQLBackend: cache set failed" << std::endl;
            }
        }
        return true;
    }
    return false;
}

bool MySQLBackend::insert_record(int key, std::vector<std::string>& values) {
    return execute_update(generate_insert_sql(key, values));
}

bool MySQLBackend::execute_update(std::string statement) {
    std::unique_ptr<sql::Statement> stmt(conn->createStatement());
    stmt->execute(statement);
    return true;
}

std::unique_ptr<sql::ResultSet> MySQLBackend::execute_query(std::string statement) {
    std::unique_ptr<sql::Statement> stmt(conn->createStatement());
    return std::unique_ptr<sql::ResultSet>(stmt->executeQuery(statement));
}

std::string MySQLBackend::generate_read_sql(int key) {
    return "SELECT * FROM " + table_name + " WHERE ycsb_key = " + std::to_string(key) + ";";
}

std::string MySQLBackend::generate_insert_sql(int key, std::vector<std::string>& values) {
    std::string sql = "INSERT INTO " + table_name + " (ycsb_key, field1, field2, field3, field4, field5, field6, field7, field8, field9, field10) VALUES (" + std::to_string(key);
    for (auto& value : values) {
        sql += ", '" + value + "'";
    }
    sql += ")";
    return sql;
}

bool MySQLBackend::clean_up() {
    // delete newly inserted records >= MAX_RECORDS
    execute_update("DELETE FROM " + table_name + " WHERE ycsb_key >= " + std::to_string(MAX_RECORDS));
    return true;
}

