import re

with open('src/apimanager.cpp', 'r') as f:
    content = f.read()

# I appended the new impl but it was already there. Let's just remove the one at the end.
content = re.sub(
    r'\nvoid APIManager::cancelGithubIssueContextFetch\(const QString &sourceId, int issueNumber\) \{\n  QString key = QStringLiteral\("%1#%2"\)\.arg\(sourceId\)\.arg\(issueNumber\);\n  if \(m_githubIssueContextReplies\.contains\(key\)\) \{\n    QNetworkReply \*reply = m_githubIssueContextReplies\.value\(key\);\n    if \(reply\) \{\n      reply->abort\(\);\n    \}\n  \}\n\}\n$',
    '\n',
    content
)

with open('src/apimanager.cpp', 'w') as f:
    f.write(content)
