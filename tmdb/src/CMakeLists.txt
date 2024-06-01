cmake_minimum_required(VERSION 3.12)
project(tmdb)
#set(CMAKE_C_COMPILER g++)

set(TD_dir "${CMAKE_CURRENT_LIST_DIR}")
message("in tmdb CMake: ${CMAKE_CURRENT_LIST_DIR}")

# 确定源文件
set(tmdb_util_hd "${TD_dir}/tm_util.h")
set(tmdb_util_sr "${TD_dir}/tm_util.cpp")
set(tmdb_hd "${TD_dir}/tmdb.h")
set(tmdb_sr "${TD_dir}/tmdb.cpp")

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
add_executable(tmdb_test ${TD_dir}/tmdb_test.cpp)
target_link_libraries(
  tmdb_test 
  tmdb
)


