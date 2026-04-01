add_executable(test_dummy test_dummy.cpp)

target_link_libraries(test_dummy Qt6::Test)
add_test(NAME DummyTest COMMAND test_dummy)
