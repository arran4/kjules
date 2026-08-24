import re

with open('src/mainwindow.h', 'r') as f:
    content = f.read()

# Fix double declaration
content = re.sub(
    r'  ClickableLabel \*m_unseenErrorLabel;\n  ClickableLabel \*m_unseenErrorLabel;',
    '  ClickableLabel *m_unseenErrorLabel;',
    content
)

# Fix onUnseenErrorsCountChanged missing declaration - we replaced onSourcesRefreshFinished with it previously! That might have been a mistake if it was still needed or if the regex matched multiple. Let's restore onSourcesRefreshFinished and add onUnseenErrorsCountChanged properly.
content = re.sub(
    r'void onSourcesRefreshFinished\(\);\n  void onUnseenErrorsCountChanged\(int count\);',
    'void onSourcesRefreshFinished();',
    content
)

# Put it in private slots
content = re.sub(
    r'void onSourcesRefreshFinished\(\);',
    'void onSourcesRefreshFinished();\n  void onUnseenErrorsCountChanged(int count);',
    content
)

with open('src/mainwindow.h', 'w') as f:
    f.write(content)
