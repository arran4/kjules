import re

with open('src/sourcewindow.cpp', 'r') as f:
    content = f.read()

# Modify SourceWindow::onGithubIssueContextFailed to handle ApiError
content = re.sub(
    r'void SourceWindow::onGithubIssueContextFailed\(const QString &sourceId, int issueNumber, const QString &error\)',
    'void SourceWindow::onGithubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error)',
    content
)

# And update sourcewindow.h
with open('src/sourcewindow.h', 'r') as fh:
    hcontent = fh.read()

hcontent = re.sub(
    r'void onGithubIssueContextFailed\(const QString &sourceId, int issueNumber, const QString &error\);',
    'void onGithubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error);',
    hcontent
)

with open('src/sourcewindow.h', 'w') as fh:
    fh.write(hcontent)
