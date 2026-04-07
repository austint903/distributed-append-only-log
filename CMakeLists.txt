cmake_minimum_required(VERSION 3.20)
project(distributed_append_only_log)

set(CMAKE_CXX_STANDARD 20)

add_executable(server src/server/main.cpp
        src/server/udp_server/udp_server.h
        src/server/udp_server/udp_server.cpp
        src/server/append_log/append_log.h
        src/server/append_log/append_log.cpp
        src/server/log_reader/log_reader.h
        src/server/log_reader/log_reader.cpp
        src/server/http_server/http_server.h
        src/server/http_server/http_server.cpp
        src/server/topic_registry/topic_registry.h
        src/server/topic_registry/topic_registry.cpp
        src/util/util.h
        src/util/util.cpp
        )
target_link_libraries(server uring pthread)

add_executable(producer tools/producer/main.cpp
        tools/producer/producer.h
        tools/producer/producer.cpp
        src/util/util.h
        src/util/util.cpp
        src/server/http_server/http_server.h)

add_executable(dlog src/util/dlog/main.cpp)

add_executable(router
        src/router/main.cpp
        src/router/partition_map.h
        src/router/partition_map.cpp
        src/router/router_server.h
        src/router/router_server.cpp
)
target_link_libraries(router pthread)

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

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# test_crc32
add_executable(test_crc32
    tests/test_crc32.cpp
    src/util/util.cpp)
target_link_libraries(test_crc32 GTest::gtest_main)
add_test(NAME test_crc32 COMMAND test_crc32)

# test_scan_log
add_executable(test_scan_log
    tests/test_scan_log.cpp
    src/util/util.cpp)
target_link_libraries(test_scan_log GTest::gtest_main)
add_test(NAME test_scan_log COMMAND test_scan_log)

# test_protocol
add_executable(test_protocol
    tests/test_protocol.cpp)
target_link_libraries(test_protocol GTest::gtest_main)
add_test(NAME test_protocol COMMAND test_protocol)

# test_producer 
add_executable(test_producer
    tests/test_producer.cpp
    tools/producer/producer.cpp)
target_link_libraries(test_producer GTest::gtest_main)
add_test(NAME test_producer COMMAND test_producer)


add_executable(test_log_reader
    tests/test_log_reader.cpp
    src/server/append_log/append_log.cpp
    src/server/log_reader/log_reader.cpp
    src/util/util.cpp)
target_link_libraries(test_log_reader GTest::gtest_main uring)
add_test(NAME test_log_reader COMMAND test_log_reader)

# test_append_log_concurrency
add_executable(test_append_log_concurrency
    tests/test_append_log_concurrency.cpp
    src/server/append_log/append_log.cpp
    src/util/util.cpp)
target_link_libraries(test_append_log_concurrency GTest::gtest_main uring pthread)
add_test(NAME test_append_log_concurrency COMMAND test_append_log_concurrency)

# test_topic_registry
add_executable(test_topic_registry
    tests/test_topic_registry.cpp
    src/server/topic_registry/topic_registry.cpp
    src/server/append_log/append_log.cpp
    src/server/log_reader/log_reader.cpp
    src/util/util.cpp)
target_link_libraries(test_topic_registry GTest::gtest_main uring pthread)
add_test(NAME test_topic_registry COMMAND test_topic_registry)

# test_tail_consumer
add_executable(test_tail_consumer
    tests/test_tail_consumer.cpp
    src/server/append_log/append_log.cpp
    src/util/util.cpp)
target_link_libraries(test_tail_consumer GTest::gtest_main uring pthread)
add_test(NAME test_tail_consumer COMMAND test_tail_consumer)
