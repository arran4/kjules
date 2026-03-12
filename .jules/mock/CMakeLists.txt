cmake_minimum_required(VERSION 3.16)
project(kjules_mock VERSION 0.1.0)

set(QT_MIN_VERSION "5.15.2")
set(KF_MIN_VERSION "5.80.0")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Gui Widgets Network
)

find_package(KF5 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive
)

add_definitions(-DMOCK_UI_TEST)

set(kjules_mock_SRCS
    mock.cpp
    ../../src/mainwindow.cpp
    ../../src/apimanager.cpp
    ../../src/backupdialog.cpp
    ../../src/restoredialog.cpp
    ../../src/sessionmodel.cpp
    ../../src/sourcemodel.cpp
    ../../src/draftsmodel.cpp
    ../../src/errorsmodel.cpp
    ../../src/newsessiondialog.cpp
    ../../src/settingsdialog.cpp
    ../../src/draftdelegate.cpp
    ../../src/queuedelegate.cpp
    ../../src/queuemodel.cpp
    ../../src/sessiondelegate.cpp
    ../../src/sessionwindow.cpp
    ../../src/errorwindow.cpp
    ../../src/sessionswindow.cpp
    ../../src/kjules.qrc
)

add_executable(kjules_mock ${kjules_mock_SRCS})

target_include_directories(kjules_mock PRIVATE ../../src)

target_link_libraries(kjules_mock
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network
    KF5::CoreAddons KF5::I18n KF5::Wallet KF5::Notifications
    KF5::XmlGui KF5::ConfigCore KF5::WidgetsAddons KF5::GlobalAccel KF5::Archive
)
