import re

with open('src/mainwindow.h', 'r') as f:
    content = f.read()

content = re.sub(
    r'void onSourcesRefreshFinished\(\);',
    'void onSourcesRefreshFinished();\n  void onUnseenErrorsCountChanged(int count);',
    content
)

with open('src/mainwindow.h', 'w') as f:
    f.write(content)
