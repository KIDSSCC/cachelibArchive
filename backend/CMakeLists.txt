cmake_minimum_required(VERSION 3.20)
project(YCSB_CPP)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
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

# Set include directories using target_include_directories
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(YCSB_CPP ${SOURCES})

target_include_directories(YCSB_CPP PRIVATE 
    ${leveldb_INCLUDE_DIRS}
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    /usr/include/cppconn # Consider verifying this path or find a package if possible
)

# Link libraries
target_link_libraries(YCSB_CPP
    PRIVATE
    mysqlcppconn
    leveldb::leveldb
    ${LIBMONGOCXX_LIBRARIES}
    Threads::Threads
    SQLite::SQLite3
)

# Handle the address sanitizer specifically for Debug builds
target_compile_options(YCSB_CPP PRIVATE "$<$<CONFIG:DEBUG>:-fsanitize=address>")
target_link_options(YCSB_CPP PRIVATE "$<$<CONFIG:DEBUG>:-fsanitize=address>")