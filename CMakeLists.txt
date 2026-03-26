cmake_minimum_required(VERSION 3.20)
project(distributed_append_only_log)

set(CMAKE_CXX_STANDARD 20)

add_executable(server src/server/main.cpp
        src/server/udp_server/udp_server.h
        src/server/udp_server/udp_server.cpp
        src/server/append_log/append_log.h
        src/server/append_log/append_log.cpp
        src/util/util.h
        src/util/util.cpp)
target_link_libraries(server uring)

add_executable(producer tools/producer/main.cpp
        tools/producer/producer.h
        tools/producer/producer.cpp
        src/util/util.h
        src/util/util.cpp
        src/server/http_server/http_server.h)

add_executable(test_append tests/test_append.cpp
        src/server/append_log/append_log.h
        src/server/append_log/append_log.cpp
        src/util/util.h
        src/util/util.cpp)
target_link_libraries(test_append uring)

add_executable(test_crash_recovery tests/test_crash_recovery.cpp
        src/server/append_log/append_log.h
        src/server/append_log/append_log.cpp
        src/util/util.h
        src/util/util.cpp)
target_link_libraries(test_crash_recovery uring)
add_executable(http_server src/server/http_server/http_server.h
        src/server/http_server/http_server.cpp)
