set(kjulesrunner_SRCS kjulesrunner.cpp kjulesrunner.h)

if (QT_MAJOR_VERSION STREQUAL "6")
    add_library(kjulesrunner MODULE ${kjulesrunner_SRCS})
    target_link_libraries(kjulesrunner Qt6::Core Qt6::DBus KF6::Runner KF6::I18n)
else()
    add_library(kjulesrunner MODULE ${kjulesrunner_SRCS})
    target_link_libraries(kjulesrunner Qt5::Core Qt5::DBus KF5::Runner KF5::I18n)
endif()

install(TARGETS kjulesrunner DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf${QT_MAJOR_VERSION}/krunner)
