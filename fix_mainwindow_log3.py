with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

import re
content = re.sub(r'ActivityLogWindow::instance\(\)->logMessage\(i18n\("Error: %1", message\)\);', '// Duplicate logging removed', content)

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
