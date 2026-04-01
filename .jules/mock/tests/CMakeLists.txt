add_executable(test_dummy test_dummy.cpp)

if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(test_dummy Qt6::Test)
else()
    target_link_libraries(test_dummy Qt5::Test)
endif()

add_test(NAME DummyTest COMMAND test_dummy)
