cmake_minimum_required(VERSION 3.20)
project(distributed_append_only_log)

set(CMAKE_CXX_STANDARD 20)

add_executable(server src/server/main.cpp
        src/util/util.h
        src/util/util.cpp)
target_link_libraries(server uring)

add_executable(producer src/producer/main.cpp
        src/util/util.h
        src/util/util.cpp)
