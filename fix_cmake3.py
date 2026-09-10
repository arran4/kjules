with open("tests/CMakeLists.txt", "r") as f:
    content = f.read()

# Fix for test_sessionswidget - it doesn't have jobstore.cpp natively in its list in HEAD.
# So we need to add them exactly once.
content = content.replace(
    "add_executable(test_sessionswidget test_sessionswidget.cpp ../src/sessionswidget.cpp ../src/sessionswindow.cpp ../src/sessionmodel.cpp ../src/sessionwindow.cpp ../src/activitybrowser.cpp",
    "add_executable(test_sessionswidget test_sessionswidget.cpp ../src/sessionswidget.cpp ../src/sessionswindow.cpp ../src/sessionmodel.cpp ../src/sessionwindow.cpp ../src/jobstore.cpp ../src/jobdata.cpp ../src/activitybrowser.cpp"
)

with open("tests/CMakeLists.txt", "w") as f:
    f.write(content)
