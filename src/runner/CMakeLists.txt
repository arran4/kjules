set(kjulesrunner_SRCS kjulesrunner.cpp kjulesrunner.h)

add_library(kjulesrunner MODULE ${kjulesrunner_SRCS})
target_link_libraries(kjulesrunner Qt6::Core Qt6::DBus KF6::Runner KF6::I18n)
install(TARGETS kjulesrunner DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/krunner)
