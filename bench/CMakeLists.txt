cmake_minimum_required(VERSION 3.16)
project(bench_tests)

set(CMAKE_AUTOMOC ON)
find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark Qt6::Core Qt6::Test)

add_executable(test_performance test_performance.cpp)
target_link_libraries(test_performance Qt6::Core Qt6::Test)

add_executable(bench_sessionmodel bench_sessionmodel.cpp)
target_link_libraries(bench_sessionmodel Qt6::Core Qt6::Test)

add_executable(bench_apimanager bench_apimanager.cpp ../src/apimanager.cpp)
target_link_libraries(bench_apimanager Qt6::Core Qt6::Test Qt6::Network KF6::Wallet)
target_include_directories(bench_apimanager PRIVATE ../src)
