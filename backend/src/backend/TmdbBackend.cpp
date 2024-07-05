#include "TmdbBackend.h"

TmdbBackend::TmdbBackend(int thread_id):Backend(thread_id) {
    disk_name = UNIFIED_CACHE_POOL;
    string idx_file_name = disk_name + ".tdi";
    string dat_file_name = disk_name + ".tdb";
    char mode[] = "w";
    if(fopen(idx_file_name.c_str(), "r")==NULL ||fopen(dat_file_name.c_str(), "r")==NULL){
        //disk_file not exist
        std::cout<<"file not exist\n";
        mode[0] = 'c';
    }
    db = tdb_open(disk_name.c_str(), mode);
    if(!db){
        std::cout<<"TMDB: error open "<<disk_name<<std::endl;
        exit(0);
    }
}

TmdbBackend::~TmdbBackend() {
    tdb_close(db);
}

bool TmdbBackend::create_database() {
    return true;
}

bool TmdbBackend::insert_record(int key, std::vector<std::string>& values) {
    // std::cout<<"insert "<<key<<" -> "<<values[0]<<std::endl;
    if(cache_enabled){
        cache.set_(std::to_string(key), values[0]);
    }
    tdb_store(db, std::to_string(key).c_str(), values[0].c_str(), TDB_INSERT);
    return true;
}

bool TmdbBackend::read_record(int key, std::vector<std::string>& results){
    // std::string ans (1000000, 'A');
    if(cache_enabled) {
        // std::cout<<"get key is: "<<key<<std::endl;
        std::string value = cache.get_(std::to_string(key));
        // std::cout<<"receive value"<< value <<std::endl;
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

bool TmdbBackend::clean_up() {
    return true;
}
