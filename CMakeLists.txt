# 原始客户端与服务端的编译
cmake_minimum_required(VERSION 3.12)

project(CachelibServer CXX)

find_package(cachelib CONFIG REQUIRED)

include_directories("/home/md/MQCachelib/")

# 编译cachelibHeader库
set(CH_dir "${CMAKE_CURRENT_SOURCE_DIR}/CachelibHeader/")
add_library(cachelibHeader "${CH_dir}/cachelibHeader.h" "/${CH_dir}/cachelibHeader.cpp")
target_link_libraries(cachelibHeader cachelib)
include_directories("${CH_dir}")

# 编译ClientAPI库
set(CA_dir "${CMAKE_CURRENT_SOURCE_DIR}/ClientAPI/")
add_library(ClientAPI "${CA_dir}/clientAPI.h" "${CA_dir}/clientAPI.cpp")
target_include_directories(ClientAPI PUBLIC MessageInstruct)
include_directories("${CA_dir}")


# 生成Server端可执行
add_executable(Server Server.cpp)
target_link_libraries(Server cachelibHeader)
target_include_directories(Server PUBLIC MessageInstruct)


include("${CMAKE_CURRENT_SOURCE_DIR}/tmdb/src/CMakeLists.txt")
# 生成Client可执行
#add_executable(Client Client.cpp)
#target_link_libraries(Client 
#    cachelibHeader
#    ClientAPI
#)