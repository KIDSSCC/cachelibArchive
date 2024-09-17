cmake_minimum_required(VERSION 3.12)
project(YCSB_CPP)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

# add leveldb to cmake_prefix_path
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake/leveldb")

# Find packages
find_package(leveldb REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(SQLite3 REQUIRED)
pkg_search_module(LIBMONGOCXX REQUIRED libmongocxx)

# 检索配置文件列表
file(GLOB CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/src/config/*.h")
message("-----")
message(STATUS "My list: ${CONFIG_FILE}")
message("-----")
# Set include directories using target_include_directories
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

# 根据配置文件列表依次进行编译
foreach(CONFIG_ ${CONFIG_FILE})
    get_filename_component(FILE_NAME ${CONFIG_} NAME)
    string(REGEX REPLACE "\\.h$" "" CONFIG_ID ${FILE_NAME})
    add_executable("bin_${CONFIG_ID}" ${SOURCES})
    # 定义编译选项
    target_compile_definitions("bin_${CONFIG_ID}" PRIVATE CONFIG_FILE="${CONFIG_}")

    target_include_directories("bin_${CONFIG_ID}" PRIVATE 
        ${leveldb_INCLUDE_DIRS}
        ${LIBMONGOCXX_INCLUDE_DIRS}
        ${CMAKE_CURRENT_LIST_DIR}/src
        /usr/include/cppconn # Consider verifying this path or find a package if possible
        ${TD_dir}
        ${CS_dir}
    )

    # 链接库
    target_link_libraries("bin_${CONFIG_ID}" PRIVATE
        mysqlcppconn
        leveldb::leveldb
        ${LIBMONGOCXX_LIBRARIES}
        Threads::Threads
        SQLite::SQLite3
        tmdb
        ClientAPI
    )

    # Handle the address sanitizer specifically for Debug builds
    target_compile_options("bin_${CONFIG_ID}" PRIVATE "$<$<CONFIG:DEBUG>:-fsanitize=address>")
    target_link_options("bin_${CONFIG_ID}" PRIVATE "$<$<CONFIG:DEBUG>:-fsanitize=address>")
endforeach()







