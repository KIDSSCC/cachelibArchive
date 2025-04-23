#include "TmdbBackend.h"

/**
 * TmdbBackend：后端构造，打开tmdb的索引与数据文件。在不存在对应文件情况下创建新的数据库
 * params：
 *      thread_id：线程编号，用于backend构造
 */
TmdbBackend::TmdbBackend(int thread_id):Backend(thread_id) {
    disk_name = string(TMDB_TABLE_PREFIX) + string(UNIFIED_CACHE_POOL);
    string idx_file_name = disk_name + ".tdi";
    string dat_file_name = disk_name + ".tdb";
    char mode[] = "w";
    if(fopen(idx_file_name.c_str(), "r")==NULL ||fopen(dat_file_name.c_str(), "r")==NULL){
        // disk_file not exist
        std::cout<<"file not exist\n";
        mode[0] = 'c';
    }
    db = tdb_open(disk_name.c_str(), mode);
    if(!db){
        std::cout<<"TMDB: error open "<<disk_name<<std::endl;
        exit(0);
    }
}

/**
 * ~TmdbBackend：析构函数
 */
TmdbBackend::~TmdbBackend() {
    tdb_close(db);
}

/**
 * create_database：功能已在构造函数内实现，直接返回
 * return：
 *      bool：创建数据库是否成功
 */
bool TmdbBackend::create_database() {
    return true;
}

/**
 * insert_record：向数据库写入数据，启用缓存下同时向缓存中写入
 * params：
 *      key：key
 *      values：多个item项，tmdb默认取[0]
 * return:
 *      bool：写入是否成功
 */
bool TmdbBackend::insert_record(int key, std::vector<std::string>& values) {
    if(cache_enabled){
        cache.set_(std::to_string(key), values[0]);
    }
    // std::cout<<"Insert, Key: "<<std::to_string(key)<<" Value: "<<values[0]<<endl;
    tdb_store(db, std::to_string(key).c_str(), values[0].c_str(), TDB_INSERT);
    
    return true;
}

/**
 * read_record：查询一条记录，优先缓存查询，后磁盘查询。磁盘查询结果写回缓存
 * params：
 *      key：key
 *      results：保存查询结果
 * return：
 *      查询是否成功
 */
bool TmdbBackend::read_record(int key, std::vector<std::string>& results){
    if(cache_enabled) {
        // std::cout<<"get key is: "<<key<<std::endl;
        std::string value = cache.get_(std::to_string(key));
        // std::cout<<"Get, Key: "<<std::to_string(key)<<" Receive Value: "<< value <<std::endl;
        if(value != ""){
            results.push_back(value);
            hit_count++;
            return true;
        }
    }
    string value_in_disk = tdb_fetch(db, std::to_string(key).c_str());
    results.push_back(value_in_disk);
    if(cache_enabled){
        cache.set_(std::to_string(key), value_in_disk);
    }
    return true;
}

/**
 * clean_up：tmdb无需清理工作
 */
bool TmdbBackend::clean_up() {
    return true;
}
