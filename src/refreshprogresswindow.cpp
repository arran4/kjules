#include "refreshprogresswindow.h"
#include "apimanager.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

RefreshProgressWindow::RefreshProgressWindow(const QStringList &sessionIds,
                                             APIManager *apiManager,
                                             QWidget *parent)
    : QDialog(parent), m_queue(sessionIds), m_totalCount(sessionIds.size()),
      m_currentIndex(0), m_processedCount(0), m_apiManager(apiManager) {
  setWindowTitle(i18n("Refresh Progress"));

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  m_maxWorkers = config.readEntry("RefreshWorkers", 3);
  resize(600, 400);

  QVBoxLayout *layout = new QVBoxLayout(this);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, m_totalCount);
  m_progressBar->setValue(0);
  layout->addWidget(m_progressBar);

  m_textBrowser = new QTextBrowser(this);
  layout->addWidget(m_textBrowser);

  m_closeButton = new QPushButton(i18n("Close"), this);
  m_closeButton->setEnabled(false); // Disable until finished
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  layout->addWidget(m_closeButton);

  connect(m_apiManager, &APIManager::sessionReloaded, this,
          &RefreshProgressWindow::onSessionReloaded);
  connect(m_apiManager, &APIManager::sessionReloadFailed, this,
          &RefreshProgressWindow::onSessionReloadFailed);
  connect(m_apiManager, &APIManager::githubPullRequestInfoReceived, this,
          &RefreshProgressWindow::onGithubPullRequestInfoReceived);
  connect(m_apiManager, &APIManager::githubPullRequestFailed, this,
          &RefreshProgressWindow::onGithubPullRequestFailed);

  // Start processing asynchronously so the UI can show up
  QMetaObject::invokeMethod(this, &RefreshProgressWindow::processNext,
                            Qt::QueuedConnection);
}

RefreshProgressWindow::~RefreshProgressWindow() {}

void RefreshProgressWindow::processNext() {
  if (!m_apiManager->canConnect()) {
    m_textBrowser->append(i18n("<b>Error:</b> Cannot refresh: No token or "
                               "previous failure. Processing stopped."));
    m_progressBar->setValue(m_totalCount); // Force completion
    m_closeButton->setEnabled(true);
    return;
  }

  if (m_processedCount >= m_totalCount) {
    m_textBrowser->append(i18n("<b>Finished.</b>"));
    m_closeButton->setEnabled(true);
    return;
  }

  while (m_activeTasks.size() < m_maxWorkers && m_currentIndex < m_totalCount) {
    QString id = m_queue[m_currentIndex];

    QString cleanId = APIManager::cleanSessionId(id);

    m_activeTasks.insert(cleanId);
    m_textBrowser->append(i18n("Refreshing session %1...", cleanId));
    m_apiManager->reloadSession(id); // Pass the original one, it gets cleaned
    m_currentIndex++;
  }
}

void RefreshProgressWindow::finishCurrentTask(const QString &id) {
  if (m_activeTasks.contains(id)) {
    m_activeTasks.remove(id);
    m_activeTasksPrUrls.remove(id);
    m_processedCount++;
    m_progressBar->setValue(m_processedCount);
    processNext();
  }
}

void RefreshProgressWindow::onSessionReloaded(const QJsonObject &session) {
  QString id = APIManager::cleanSessionId(session.value(QStringLiteral("id")).toString());
  if (m_activeTasks.contains(id)) {
    m_textBrowser->append(i18n(
        "<font color='green'>Successfully reloaded session %1.</font>", id));

    QString prUrl;
    if (session.contains(QStringLiteral("pullRequest"))) {
      QJsonObject prObj = session.value(QStringLiteral("pullRequest")).toObject();
      prUrl = prObj.value(QStringLiteral("url")).toString();
    }

    if (!prUrl.isEmpty()) {
      m_textBrowser->append(i18n("Fetching GitHub PR info for %1...", id));
      m_activeTasksPrUrls.insert(id, prUrl);
      m_apiManager->fetchGithubPullRequest(prUrl);
    } else {
      finishCurrentTask(id);
    }
  }
}

void RefreshProgressWindow::onGithubPullRequestInfoReceived(const QString &prUrl, const QJsonObject &info) {
  Q_UNUSED(info);
  QString idToFinish;
  for (auto it = m_activeTasksPrUrls.begin(); it != m_activeTasksPrUrls.end(); ++it) {
    if (it.value() == prUrl) {
      idToFinish = it.key();
      break;
    }
  }

  if (!idToFinish.isEmpty()) {
    m_textBrowser->append(i18n(
        "<font color='green'>Successfully fetched PR info for %1.</font>", idToFinish));
    finishCurrentTask(idToFinish);
  }
}

void RefreshProgressWindow::onGithubPullRequestFailed(const QString &prUrl, const QString &message) {
  QString idToFinish;
  for (auto it = m_activeTasksPrUrls.begin(); it != m_activeTasksPrUrls.end(); ++it) {
    if (it.value() == prUrl) {
      idToFinish = it.key();
      break;
    }
  }

  if (!idToFinish.isEmpty()) {
    m_textBrowser->append(i18n(
        "<font color='orange'>GitHub fetch failed for %1: %2</font>", idToFinish, message));
    finishCurrentTask(idToFinish);
  }
}

void RefreshProgressWindow::onSessionReloadFailed(const QString &sessionId, const QString &message) {
  QString cleanId = APIManager::cleanSessionId(sessionId);
  if (m_activeTasks.contains(cleanId)) {
    m_textBrowser->append(
        i18n("<font color='red'>Failed to reload session %1: %2</font>", cleanId,
             message));
    finishCurrentTask(cleanId);
  }
}
