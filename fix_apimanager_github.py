import re

with open('src/apimanager.h', 'r') as f:
    content = f.read()

# Change githubIssueContextFailed to carry ApiError
content = re.sub(
    r'void githubIssueContextFailed\(const QString &sourceId, int issueNumber, const QString &error\);',
    'void githubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error);',
    content
)

# Add cancel mechanism
content = re.sub(
    r'void fetchGithubIssueContext\(const QString &sourceId, int issueNumber\);',
    'void fetchGithubIssueContext(const QString &sourceId, int issueNumber);\n  void cancelGithubIssueContextFetch(const QString &sourceId, int issueNumber);',
    content
)

with open('src/apimanager.h', 'w') as f:
    f.write(content)


with open('src/apimanager.cpp', 'r') as f:
    content = f.read()

# Implement cancelGithubIssueContextFetch
cancel_impl = '''
void APIManager::cancelGithubIssueContextFetch(const QString &sourceId, int issueNumber) {
  QString key = QStringLiteral("%1#%2").arg(sourceId).arg(issueNumber);
  if (m_githubIssueContextReplies.contains(key)) {
    QNetworkReply *reply = m_githubIssueContextReplies.value(key);
    if (reply) {
      reply->abort();
    }
  }
}
'''
content += cancel_impl

with open('src/apimanager.cpp', 'w') as f:
    f.write(content)
