cmake_minimum_required(VERSION 3.16)

project(kjules VERSION 0.0.31)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MAJOR_VERSION 6)

set(QT_DEFAULT_MAJOR_VERSION ${QT_MAJOR_VERSION})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_VERSION_MAJOR ${QT_MAJOR_VERSION})
set(KF_MAJOR_VERSION ${QT_MAJOR_VERSION})

# Try to determine version from git
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_VERSION)
        set(KJULES_VERSION_STRING "${GIT_VERSION}")
    endif()
endif()

if(NOT DEFINED KJULES_VERSION_STRING)
    set(KJULES_VERSION_STRING "${PROJECT_VERSION}")
endif()

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets Network Test)
find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(FeatureSummary)

option(USE_MOCK_API "Build with mock API support" OFF)
option(DEV_MODE "Enable development mode (stores data in project dir)" OFF)

add_definitions(-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_URL_CAST_FROM_STRING
                -DQT_NO_KEYWORDS)

if(USE_MOCK_API)
    add_definitions(-DUSE_MOCK_API)
endif()

if(DEV_MODE)
    add_definitions(-DDEV_MODE)
    add_definitions(-DKJULES_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
endif()

set(kjules_SRCS
    src/main.cpp
    src/mainwindow.cpp
    src/apimanager.cpp
    src/followsessiondialog.cpp
    src/backupdialog.cpp
    src/restoredialog.cpp
    src/sessionmodel.cpp
    src/sourcemodel.cpp
    src/draftsmodel.cpp
    src/templatesmodel.cpp
    src/blockedtreemodel.cpp
    src/errorsmodel.cpp
    src/newsessiondialog.cpp
    src/savedialog.cpp
    src/templateselectiondialog.cpp
    src/templateeditdialog.cpp
    src/settingsdialog.cpp
    src/draftdelegate.cpp
    src/queuedelegate.cpp
    src/queuemodel.cpp
    src/activitybrowser.cpp
    src/activitylogwindow.cpp
    src/clickableprogressbar.cpp
    src/sessiondelegate.cpp
    src/sessionwindow.cpp
    src/errorwindow.cpp
    src/sessionswindow.cpp
    src/kjules.qrc
    src/filterparser.cpp
    src/filtereditor.cpp
    src/advancedfilterproxymodel.cpp
    src/refreshprogresswindow.cpp
    src/sourcesrefreshprogresswindow.cpp
)

add_executable(kjules ${kjules_SRCS})

target_compile_definitions(kjules PRIVATE "KJULES_VERSION=\"${KJULES_VERSION_STRING}\"")

target_link_libraries(kjules
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    KF6::CoreAddons
    KF6::I18n
    KF6::Wallet
    KF6::Notifications
    KF6::XmlGui
    KF6::ConfigCore
    KF6::WidgetsAddons
    KF6::GlobalAccel
    KF6::Archive
)

install(TARGETS kjules ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kjules.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kjules.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/sc-apps-kjules.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES src/kjulesui.rc src/sessionswindowui.rc src/newsessiondialogui.rc DESTINATION ${KDE_INSTALL_KXMLGUI6DIR}/kjules)
install(FILES src/kjules.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

enable_testing()
add_subdirectory(tests)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
add_subdirectory(bench)
