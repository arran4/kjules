with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

import re
content = re.sub(r'm_activityLogWindow->logError\(message\);\s*// Duplicate logging', '// Duplicate logging removed', content)
content = re.sub(r'm_activityLogWindow->logError\(message\);', '// updateStatus already logs it', content)

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
