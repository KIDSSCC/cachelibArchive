cmake_minimum_required(VERSION 3.12)
project(CachelibServer CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
message("-- Detecting C++ standard ${CMAKE_CXX_STANDARD}")

find_package(cachelib CONFIG REQUIRED)

# some paths
set(CH_dir "${CMAKE_SOURCE_DIR}/clientserver/cachelibheader")
set(CS_dir "${CMAKE_SOURCE_DIR}/clientserver/clientserver")
set(CS_config "${CMAKE_SOURCE_DIR}/clientserver")

# set cachelibheader library
add_library(CachelibHeader STATIC "${CH_dir}/cachelibHeader.cpp")
target_include_directories(CachelibHeader PUBLIC "${CS_config}")
target_link_libraries(CachelibHeader PUBLIC cachelib)


# set client api library
add_library(ClientAPI STATIC "${CS_dir}/clientAPI.cpp")

# generate server executable
add_executable(Server "${CS_dir}/server.cpp")
target_link_libraries(Server PRIVATE CachelibHeader)
target_include_directories(Server PRIVATE "${CH_dir}" PRIVATE "${CS_config}")


# call tmdb cmake
include("${CMAKE_SOURCE_DIR}/tmdb/src/CMakeLists.txt")

# call backend cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/backend/CMakeLists.txt")


# 生成SharedMemory_test测试
# add_executable(Client shm_test.cpp)
# target_link_libraries(Client 
#   ClientAPI
# )