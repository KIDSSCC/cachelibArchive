cmake_minimum_required(VERSION 3.12)
project(tmdb)

set(CMAKE_C_COMPILER g++)
set(FILE_PATH /home/md/SHMCachelib/tmdb/src)


# 确定源文件
set(tmdb_util_hd "${FILE_PATH}/tm_util.h")
set(tmdb_util_sr "${FILE_PATH}/tm_util.c")
set(tmdb_hd "${FILE_PATH}/tmdb.h")
set(tmdb_sr "${FILE_PATH}/tmdb.c")

add_library(tmdb
 ${tmdb_util_hd}
 ${tmdb_util_sr}
 ${tmdb_hd}
 ${tmdb_sr}
 )

include_directories("${CA_dir}")
target_link_libraries(tmdb 
  ClientAPI 
 )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
add_executable(tmdb_test ${FILE_PATH}/tmdb_test.c)
target_link_libraries(
  tmdb_test 
  tmdb
)


