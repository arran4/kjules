add_executable(test_dummy test_dummy.cpp)
add_executable(test_filter test_filter.cpp ../src/filterparser.cpp ../src/filtereditor.cpp)
add_executable(test_advancedfilterproxymodel test_advancedfilterproxymodel.cpp ../src/advancedfilterproxymodel.cpp ../src/filterparser.cpp ../src/sessionmodel.cpp ../src/sourcemodel.cpp)

target_link_libraries(test_dummy Qt6::Test)
target_link_libraries(test_filter Qt6::Test Qt6::Widgets Qt6::Core)
target_link_libraries(test_advancedfilterproxymodel Qt6::Test Qt6::Widgets Qt6::Core KF6::I18n KF6::ConfigWidgets KF6::ConfigCore KF6::CoreAddons)

add_test(NAME DummyTest COMMAND test_dummy)
add_test(NAME FilterTest COMMAND test_filter)
add_test(NAME AdvancedFilterProxyModelTest COMMAND test_advancedfilterproxymodel)
