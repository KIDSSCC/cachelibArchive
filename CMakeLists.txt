# 原始客户端与服务端的编译
cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)
message("C++ standard: ${CMAKE_CXX_STANDARD}")

project(CachelibServer CXX)

find_package(cachelib CONFIG REQUIRED)

include_directories("/home/md/SHMCachelib/")

# 编译cachelibHeader库
set(CH_dir "${CMAKE_CURRENT_SOURCE_DIR}/CachelibHeader/")
add_library(cachelibHeader "${CH_dir}/cachelibHeader.h" "/${CH_dir}/cachelibHeader.cpp")
target_link_libraries(cachelibHeader cachelib)
include_directories("${CH_dir}")

# 编译MessageQueue_ClientAPI库
# set(CA_dir "${CMAKE_CURRENT_SOURCE_DIR}/ClientAPI/")
# 编译SharedMemory_ClientAPI库
set(CA_dir "${CMAKE_CURRENT_SOURCE_DIR}/sharedMem/ClientAPI")
add_library(ClientAPI "${CA_dir}/clientAPI.h" "${CA_dir}/clientAPI.cpp")
target_include_directories(ClientAPI PUBLIC sharedMem)
include_directories("${CA_dir}")



# 生成MessageQueue_Server端可执行
# add_executable(Server Server.cpp)
# 生成SharedMemory_Server端
add_executable(Server "${CMAKE_CURRENT_SOURCE_DIR}/sharedMem/server.cpp")
target_link_libraries(Server cachelibHeader)
target_include_directories(Server PUBLIC sharedMem)


# 调用tmdb编译子模块
include("${CMAKE_CURRENT_SOURCE_DIR}/tmdb/src/CMakeLists.txt")

# 生成MessageQueue_Client测试
#add_executable(Client Client.cpp)
#target_link_libraries(Client 
#    cachelibHeader
#    ClientAPI
#)

# 生成SharedMemory_test测试
# add_executable(Client shm_test.cpp)
# target_link_libraries(Client 
#   ClientAPI
# )
