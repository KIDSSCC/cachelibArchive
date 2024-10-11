cmake_minimum_required(VERSION 3.12)
project(tmdb)

# some paths
set(TD_dir "${CMAKE_CURRENT_LIST_DIR}")

# source files
set(tmdb_util_sr "${TD_dir}/tm_util.cpp")
set(tmdb_sr "${TD_dir}/tmdb.cpp")

#set tmdb library
add_library(tmdb ${tmdb_util_sr} ${tmdb_sr})
target_include_directories(tmdb PUBLIC "${TD_dir}" PUBLIC "${CS_dir}")
target_link_libraries(tmdb PRIVATE ClientAPI)

#generate tmdb_test executable
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
add_executable(tmdb_test "${TD_dir}/tmdb_test.cpp")
target_include_directories(tmdb_test PUBLIC "${TD_dir}" PRIVATE "${CS_dir}")
target_link_libraries(tmdb_test PUBLIC tmdb PRIVATE ClientAPI)


