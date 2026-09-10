import re

with open("tests/CMakeLists.txt", "r") as f:
    content = f.read()

# For test_sourcewindow: replace the duplicate insertions
content = re.sub(r'\.\./src/sessionwindow\.cpp\s+\.\./src/jobstore\.cpp\s+\.\./src/jobdata\.cpp\s+\.\./src/activitybrowser\.cpp', '../src/sessionwindow.cpp ../src/activitybrowser.cpp', content)

with open("tests/CMakeLists.txt", "w") as f:
    f.write(content)
