add_executable(test_dummy test_dummy.cpp)
add_executable(test_filter test_filter.cpp ../src/filterparser.cpp ../src/filtereditor.cpp)

if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(test_dummy Qt6::Test)
    target_link_libraries(test_filter Qt6::Test Qt6::Widgets Qt6::Core)
else()
    target_link_libraries(test_dummy Qt5::Test)
    target_link_libraries(test_filter Qt5::Test Qt5::Widgets Qt5::Core)
endif()

add_test(NAME DummyTest COMMAND test_dummy)
add_test(NAME FilterTest COMMAND test_filter)
