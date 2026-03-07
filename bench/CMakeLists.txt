cmake_minimum_required(VERSION 3.16)
project(bench_tests)

set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Core Test REQUIRED)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark Qt5::Core Qt5::Test)

add_executable(test_performance test_performance.cpp)
target_link_libraries(test_performance Qt5::Core Qt5::Test)

add_executable(bench_sessionmodel bench_sessionmodel.cpp)
target_link_libraries(bench_sessionmodel Qt5::Core Qt5::Test)
