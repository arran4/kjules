import re

with open('src/sourcewindow.cpp', 'r') as f:
    content = f.read()

impl = '''void SourceWindow::onGithubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error) {
  if (sourceId != m_sourceId)
    return;

  if (m_createSessionFromIssueAction) {
    m_createSessionFromIssueAction->setText(tr("Create Session..."));
    m_createSessionFromIssueAction->setEnabled(true);
  }

  if (error.type() == ApiError::Type::Canceled) {
    Q_EMIT statusMessage(tr("GitHub issue fetch cancelled"));
    return;
  }

  // Determine concise status message
  QString statusMsg;
  switch (error.type()) {
    case ApiError::Type::Authentication:
      statusMsg = tr("Authentication failed — check credentials");
      break;
    case ApiError::Type::PermissionDenied:
      statusMsg = tr("GitHub access denied — check repository/token permissions");
      break;
    case ApiError::Type::RateLimit:
      statusMsg = tr("GitHub rate limit reached — retry later"); // Or extract retry-after if available
      break;
    case ApiError::Type::NotFound:
      statusMsg = tr("GitHub issue #%1 could not be loaded").arg(issueNumber);
      break;
    case ApiError::Type::ServerError:
      statusMsg = tr("Service unavailable — try again later");
      break;
    default:
      statusMsg = tr("Failed to fetch issue context for #%1").arg(issueNumber);
      break;
  }

  Q_EMIT statusMessage(statusMsg);

  if (m_errorsModel) {
    QJsonObject errorObj = error.toJson();
    errorObj[QStringLiteral("sourceId")] = sourceId;
    errorObj[QStringLiteral("issueNumber")] = issueNumber;
    errorObj[QStringLiteral("operation")] = QStringLiteral("fetch GitHub issue context");
    errorObj[QStringLiteral("provider")] = QStringLiteral("GitHub");
    errorObj[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    // Remove the message overwrite if ApiError already has one, or use a rich one
    if (errorObj.value(QStringLiteral("message")).toString().isEmpty()) {
       errorObj[QStringLiteral("message")] = tr("Failed to fetch issue context for #%1: %2").arg(issueNumber).arg(error.message());
    }
    m_errorsModel->addErrorObj(errorObj);
  }
}'''

content = re.sub(
    r'void SourceWindow::onGithubIssueContextFailed\(const QString &sourceId, int issueNumber, const QString &error\) \{.*?\n\}\n',
    impl + '\n',
    content,
    flags=re.DOTALL
)

with open('src/sourcewindow.cpp', 'w') as f:
    f.write(content)
