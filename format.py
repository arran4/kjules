import subprocess

files = ["src/createrepodialog.cpp"]

for f in files:
    subprocess.run(["clang-format", "-i", f])
