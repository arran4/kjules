#include "sourcesrefreshprogresswindow.h"
#include "apimanager.h"
#include <QDateTime>
#include <QDialogButtonBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

SourcesRefreshProgressWindow::SourcesRefreshProgressWindow(
    APIManager *apiManager, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager), m_totalGithubRequests(0),
      m_finishedGithubRequests(0) {
  setWindowTitle(tr("Sources Refresh Progress"));
  resize(600, 400);

  QVBoxLayout *layout = new QVBoxLayout(this);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setMinimum(0);
  m_progressBar->setMaximum(0); // Indeterminate initially
  layout->addWidget(m_progressBar);

  m_textBrowser = new QTextBrowser(this);
  m_textBrowser->setOpenExternalLinks(true);
  layout->addWidget(m_textBrowser);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
  m_closeButton = new QPushButton(tr("Close"), this);
  buttonBox->addButton(m_closeButton, QDialogButtonBox::AcceptRole);
  layout->addWidget(buttonBox);

  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

  connect(m_apiManager, &APIManager::sourcesReceived, this,
          &SourcesRefreshProgressWindow::onSourcesReceived);
  connect(m_apiManager, &APIManager::githubInfoReceived, this,
          &SourcesRefreshProgressWindow::onGithubInfoReceived);
  connect(m_apiManager, &APIManager::githubInfoFailed, this,
          &SourcesRefreshProgressWindow::onGithubInfoFailed);
  connect(m_apiManager, &APIManager::sourcesRefreshFinished, this,
          &SourcesRefreshProgressWindow::onSourcesRefreshFinished);
  connect(m_apiManager, &APIManager::logMessage, this,
          &SourcesRefreshProgressWindow::appendLog);
}

void SourcesRefreshProgressWindow::appendLog(const QString &msg) {
  QString timeStr =
      QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
  m_textBrowser->append(QStringLiteral("[%1] %2").arg(timeStr, msg));
}

void SourcesRefreshProgressWindow::setProgress(int current, int total) {
  m_progressBar->setMaximum(total);
  m_progressBar->setValue(current);
}

void SourcesRefreshProgressWindow::reset() {
  m_totalGithubRequests = 0;
  m_finishedGithubRequests = 0;
  m_progressBar->setMaximum(0);
  m_progressBar->setValue(0);
  m_textBrowser->clear();
  appendLog(tr("Starting refresh..."));
}

void SourcesRefreshProgressWindow::onSourcesReceived(
    const QJsonArray &sources) {
  appendLog(tr("Loaded page with %1 sources.").arg(sources.size()));
  if (!m_apiManager->githubToken().isEmpty()) {
    for (int i = 0; i < sources.size(); ++i) {
      QJsonObject source = sources[i].toObject();
      QString id = source.value(QStringLiteral("id")).toString();
      if (id.startsWith(QStringLiteral("sources/github/"))) {
        m_totalGithubRequests++;
      }
    }
  }
  setProgress(m_finishedGithubRequests, m_totalGithubRequests);
}

void SourcesRefreshProgressWindow::onGithubInfoReceived(
    const QString &sourceId, const QJsonObject &info) {
  m_finishedGithubRequests++;
  appendLog(tr("Fetched GitHub info for %1 successfully.").arg(sourceId));
  setProgress(m_finishedGithubRequests, m_totalGithubRequests);
}

void SourcesRefreshProgressWindow::onGithubInfoFailed(const QString &sourceId,
                                                      const QString &message) {
  m_finishedGithubRequests++;
  appendLog(tr("ERROR: Failed to fetch GitHub info for %1: %2")
                .arg(sourceId, message));
  setProgress(m_finishedGithubRequests, m_totalGithubRequests);
}

void SourcesRefreshProgressWindow::onSourcesRefreshFinished() {
  appendLog(tr("Finished refreshing sources from API. Waiting for GitHub info "
               "to complete..."));
  if (m_totalGithubRequests == 0) {
    setProgress(1, 1);
  } else {
    setProgress(m_finishedGithubRequests, m_totalGithubRequests);
  }
}
