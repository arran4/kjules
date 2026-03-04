add_executable(test_dummy test_dummy.cpp)

target_link_libraries(test_dummy Qt5::Test)
add_test(NAME DummyTest COMMAND test_dummy)
add_executable(test_performance test_performance.cpp)
target_link_libraries(test_performance Qt5::Test)
add_test(NAME PerformanceTest COMMAND test_performance)
