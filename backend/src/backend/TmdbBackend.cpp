#include "TmdbBackend.h"
#include "utils/cache_utils.h"

TmdbBackend::TmdbBackend(int thread_id):Backend(thread_id) {
    disk_name = TMDB_TABLE_PREFIX;
}

TmdbBackend::~TmdbBackend() {
    tdb_close(db);
}

TmdbBackend::create_database() {
    string idx_file_name = disk_name + ".tdi";
    string dat_file_name = disk_name + ".tdb";
    char mode[] = "w";
    if(fopen(idx_file_name.c_str(), "r")==NULL ||fopen(dat_file_name.c_str(), "r")==NULL){
        //disk_file not exist
        mode[0] = 'c';
    }
    db = tdb_open(disk_name, mode);
    if(!db){
        std::cout<<"TMDB: error open "<<disk_name<<std::endl;
        exit(0);
    }
}

TmdbBackend::insert_record(int key, std::vector<std::string>& values) {
    tdb_store(db, std::to_string(key).c_str(), values[0].c_str(), TDB_INSERT);
    return true;
}