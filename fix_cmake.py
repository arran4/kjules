with open("tests/CMakeLists.txt", "r") as f:
    content = f.read()

content = content.replace(" ../src/sessionwindow.cpp\n    ../src/jobstore.cpp\n    ../src/jobdata.cpp", " ../src/sessionwindow.cpp")

with open("tests/CMakeLists.txt", "w") as f:
    f.write(content)
