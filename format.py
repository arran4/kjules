import subprocess

files = ["src/createrepodialog.h", "src/createrepodialog.cpp", "src/mainwindow.h", "src/apimanager.cpp", "src/mainwindow.cpp", "src/apimanager.h"]

for f in files:
    subprocess.run(["clang-format", "-i", f])
