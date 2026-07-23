#!/bin/bash
zypper in -y cmake gcc-c++ kf6-extra-cmake-modules qt6-base-devel qt6-tools-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kwallet-devel kf6-knotifications-devel kf6-kxmlgui-devel kf6-kconfig-devel kf6-kwidgetsaddons-devel kf6-kglobalaccel-devel kf6-karchive-devel git libqt5-qtbase-devel libQt5Core-devel dbus-1 xvfb-run
cmake -B build -S . && cmake --build build
