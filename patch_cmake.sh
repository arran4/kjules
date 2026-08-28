sed -i 's/find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive)/find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n Wallet Notifications XmlGui Config WidgetsAddons GlobalAccel Archive DBusAddons)/' CMakeLists.txt
sed -i '/KF6::Archive/a \    KF6::DBusAddons' CMakeLists.txt
