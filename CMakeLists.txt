cmake_minimum_required(VERSION 3.16)

project(kjules VERSION 0.1.0)

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
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Test
)

find_package(KF5 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    Wallet
    Notifications
    XmlGui
    Config
    WidgetsAddons
    GlobalAccel
)

add_definitions(-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_URL_CAST_FROM_STRING
                -DQT_NO_KEYWORDS)

set(kjules_SRCS
    src/main.cpp
    src/mainwindow.cpp
    src/apimanager.cpp
    src/sessionmodel.cpp
    src/sourcemodel.cpp
    src/draftsmodel.cpp
    src/newsessiondialog.cpp
    src/settingsdialog.cpp
    src/draftdelegate.cpp
    src/sessiondelegate.cpp
)

add_executable(kjules ${kjules_SRCS})

target_link_libraries(kjules
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    KF5::CoreAddons
    KF5::I18n
    KF5::Wallet
    KF5::Notifications
    KF5::XmlGui
    KF5::ConfigCore
    KF5::WidgetsAddons
    KF5::GlobalAccel
)

install(TARGETS kjules ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kjules.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES icons/sc-apps-kjules.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES src/kjulesui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kjules)

enable_testing()
add_subdirectory(tests)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
