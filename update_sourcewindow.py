import re

with open('src/sourcewindow.cpp', 'r') as f:
    content = f.read()

# Modify SourceWindow to use the structured error pipeline instead of building a QJsonObject manually

# Find onGithubIssueContextFailed
old_fail = '''  // Add to detailed errors list
  if (m_errorsModel) {
    QJsonObject errorObj;
    errorObj[QStringLiteral("message")] = tr("Failed to fetch issue context for #%1: %2").arg(issueNumber).arg(error);
    errorObj[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    errorObj[QStringLiteral("sourceId")] = sourceId;
    errorObj[QStringLiteral("issueNumber")] = issueNumber;
    m_errorsModel->addErrorObj(errorObj);
  }'''

new_fail = '''  // We are now receiving structured ApiError, but wait, the signature in sourcewindow is `const QString &error`.
  // The prompt asks to "Prefer changing the GitHub issue-context failure path to carry ApiError rather than just QString".
'''
