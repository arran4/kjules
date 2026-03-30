cmake_minimum_required(VERSION 3.16)

project(kjules VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


set(QT_MAJOR_VERSION 6 CACHE STRING "Qt major version to use")
set(KF_MAJOR_VERSION ${QT_MAJOR_VERSION})

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)

include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(FeatureSummary)

# Support Qt6 and KF6, falling back to Qt5 and KF5 if not found.
if(QT_MAJOR_VERSION STREQUAL "6")
    find_package(Qt6 6.0.0 CONFIG REQUIRED COMPONENTS Core Gui Widgets Network Test Qml)
    find_package(KF6 6.0.0 REQUIRED COMPONENTS CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive)
else()
    find_package(Qt5 5.15.2 CONFIG REQUIRED COMPONENTS Core Gui Widgets Network Test)
    find_package(KF5 5.80.0 REQUIRED COMPONENTS CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive)
endif()

add_definitions(-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_URL_CAST_FROM_STRING
                -DQT_NO_KEYWORDS)

set(kjules_SRCS
    src/main.cpp
    src/mainwindow.cpp
    src/apimanager.cpp
    src/backupdialog.cpp
    src/restoredialog.cpp
    src/sessionmodel.cpp
    src/sourcemodel.cpp
    src/draftsmodel.cpp
    src/templatesmodel.cpp
    src/errorsmodel.cpp
    src/newsessiondialog.cpp
    src/savedialog.cpp
    src/templateselectiondialog.cpp
    src/templateeditdialog.cpp
    src/settingsdialog.cpp
    src/draftdelegate.cpp
    src/queuedelegate.cpp
    src/queuemodel.cpp
    src/sessiondelegate.cpp
    src/sessionwindow.cpp
    src/errorwindow.cpp
    src/sessionswindow.cpp
    src/kjules.qrc
)

add_executable(kjules ${kjules_SRCS})

target_link_libraries(kjules
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Network
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Wallet
    KF${KF_MAJOR_VERSION}::Notifications
    KF${KF_MAJOR_VERSION}::XmlGui
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::WidgetsAddons
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::Archive
)

install(TARGETS kjules ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kjules.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES icons/sc-apps-kjules.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES src/kjulesui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kjules)

enable_testing()
add_subdirectory(tests)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
add_subdirectory(bench)
