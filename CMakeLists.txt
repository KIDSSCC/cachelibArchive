cmake_minimum_required(VERSION 3.12)
project(CachelibServer CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
message("-- Detecting C++ standard ${CMAKE_CXX_STANDARD}")

# 检查 gcc 版本
execute_process(
    COMMAND gcc --version
    OUTPUT_VARIABLE GCC_VERSION_OUTPUT
    ERROR_VARIABLE GCC_VERSION_ERROR
    RESULT_VARIABLE GCC_VERSION_RESULT
)

# 检查 g++ 版本
execute_process(
    COMMAND g++ --version
    OUTPUT_VARIABLE GPP_VERSION_OUTPUT
    ERROR_VARIABLE GPP_VERSION_ERROR
    RESULT_VARIABLE GPP_VERSION_RESULT
)

# 打印输出结果（可选）
message(STATUS "GCC version: ${GCC_VERSION_OUTPUT}")
message(STATUS "G++ version: ${GPP_VERSION_OUTPUT}")

find_package(cachelib CONFIG REQUIRED)

# some paths
set(CH_dir "${CMAKE_SOURCE_DIR}/clientserver/cachelibheader")
set(CS_dir "${CMAKE_SOURCE_DIR}/clientserver/clientserver")
set(DCP_dir "${CMAKE_SOURCE_DIR}/clientserver/DCP")
set(CS_config "${CMAKE_SOURCE_DIR}/clientserver")

# set cachelibheader library
add_library(CachelibHeader STATIC "${CH_dir}/cachelibHeader.cpp")
target_include_directories(CachelibHeader PUBLIC "${CS_config}")
target_link_libraries(CachelibHeader PUBLIC cachelib)


# set client api library
add_library(ClientAPI STATIC "${CS_dir}/clientAPI.cpp")
target_include_directories(ClientAPI PUBLIC "${CS_config}")
target_link_libraries(ClientAPI PUBLIC rt)

# generate server executable
add_executable(Server "${CS_dir}/server.cpp")
target_link_libraries(Server PRIVATE CachelibHeader)
target_include_directories(Server PRIVATE "${CH_dir}" PRIVATE "${DCP_dir}" PRIVATE "${CS_config}")


# call tmdb cmake
include("${CMAKE_SOURCE_DIR}/tmdb/src/CMakeLists.txt")

# call backend cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/backend/CMakeLists.txt")

