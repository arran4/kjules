#include "refreshprogresswindow.h"
#include "apimanager.h"
#include "mainwindow.h"
#include "sessionmodel.h"

#include <KLocalizedString>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

RefreshProgressWindow::RefreshProgressWindow(const QStringList &sessionIds,
                                             APIManager *apiManager,
                                             SessionModel *sessionModel,
                                             QWidget *parent)
    : QDialog(parent), m_queue(sessionIds), m_totalCount(sessionIds.size()),
      m_currentIndex(0), m_apiManager(apiManager), m_sessionModel(sessionModel),
      m_isFinished(false) {
  setWindowTitle(i18n("Refresh Progress"));
  resize(600, 400);

  QVBoxLayout *layout = new QVBoxLayout(this);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, m_totalCount);
  m_progressBar->setValue(0);
  layout->addWidget(m_progressBar);

  m_textBrowser = new QTextBrowser(this);
  m_textBrowser->setOpenLinks(false);
  connect(m_textBrowser, &QTextBrowser::anchorClicked, this,
          &RefreshProgressWindow::onAnchorClicked);
  layout->addWidget(m_textBrowser);

  m_closeButton = new QPushButton(i18n("Close"), this);
  m_closeButton->setEnabled(false); // Disable until finished
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  layout->addWidget(m_closeButton);

  connect(m_apiManager, &APIManager::sessionReloaded, this,
          &RefreshProgressWindow::onSessionReloaded);
  connect(m_apiManager, &APIManager::errorOccurred, this,
          &RefreshProgressWindow::onErrorOccurred);

  MainWindow *mainWindow = qobject_cast<MainWindow *>(parent);
  if (mainWindow) {
    connect(mainWindow, &MainWindow::sessionAutoArchived, this,
            &RefreshProgressWindow::onSessionAutoArchived);
  }

  // Start processing asynchronously so the UI can show up
  QMetaObject::invokeMethod(this, &RefreshProgressWindow::processNext,
                            Qt::QueuedConnection);
}

RefreshProgressWindow::~RefreshProgressWindow() {}

void RefreshProgressWindow::addSessionIds(const QStringList &ids) {
  m_queue.append(ids);
  m_totalCount += ids.size();
  m_progressBar->setMaximum(m_totalCount);

  // If we were finished, we need to restart processing
  if (m_isFinished && !ids.isEmpty()) {
    m_isFinished = false;
    m_closeButton->setEnabled(false);
    QMetaObject::invokeMethod(this, &RefreshProgressWindow::processNext,
                              Qt::QueuedConnection);
  }
}

void RefreshProgressWindow::processNext() {
  Q_EMIT progressUpdated(m_currentIndex, m_totalCount);

  if (!m_apiManager->canConnect()) {
    m_textBrowser->append(i18n("<b>Error:</b> Cannot refresh: No token or "
                               "previous failure. Processing stopped."));
    m_progressBar->setValue(m_totalCount); // Force completion
    m_closeButton->setEnabled(true);
    m_isFinished = true;
    Q_EMIT progressFinished();
    return;
  }

  if (m_currentIndex >= m_totalCount) {
    m_textBrowser->append(i18n("<b>Finished.</b>"));
    m_closeButton->setEnabled(true);
    m_isFinished = true;
    Q_EMIT progressFinished();
    return;
  }

  QString id = m_queue[m_currentIndex];
  QString link = getSessionLink(id);
  m_textBrowser->append(i18n("Refreshing session %1...", link));
  m_apiManager->reloadSession(id);
}

void RefreshProgressWindow::onAnchorClicked(const QUrl &url) {
  if (url.scheme() == QStringLiteral("session")) {
    Q_EMIT openSessionRequested(url.path());
  }
}

QString RefreshProgressWindow::getSessionLink(const QString &id) const {
  const QString escapedId = id.toHtmlEscaped();
  if (!m_sessionModel)
    return escapedId;

  const QString name = m_sessionModel->getSessionName(id);
  if (name.isEmpty()) {
    return QStringLiteral("<a href=\"session:%1\">%2</a>").arg(escapedId, escapedId);
  }
  return QStringLiteral("<a href=\"session:%1\" title=\"%2\">%3</a>")
      .arg(escapedId, name.toHtmlEscaped(), escapedId);
}

void RefreshProgressWindow::onSessionReloaded(const QJsonObject &session) {
  QString id = session.value(QStringLiteral("id")).toString();
  // Verify it's the one we are waiting for just in case
  if (m_currentIndex < m_totalCount && id == m_queue[m_currentIndex]) {
    QString link = getSessionLink(id);
    m_textBrowser->append(i18n(
        "<font color='green'>Successfully reloaded session %1.</font>", link));
    m_currentIndex++;
    m_progressBar->setValue(m_currentIndex);
    processNext();
  }
}

void RefreshProgressWindow::onSessionAutoArchived(const QString &id,
                                                  const QString &reason) {
  QString link = getSessionLink(id);
  m_textBrowser->append(i18n(
      "<font color='orange'>Session %1 auto-archived: %2</font>", link, reason));
}

void RefreshProgressWindow::onErrorOccurred(const QString &message) {
  // The APIManager emits this for any error. We assume it correlates to the
  // current item being processed.
  if (m_currentIndex < m_totalCount) {
    QString id = m_queue[m_currentIndex];
    QString link = getSessionLink(id);
    m_textBrowser->append(
        i18n("<font color='red'>Failed to reload session %1: %2</font>", link,
             message));
    m_currentIndex++;
    m_progressBar->setValue(m_currentIndex);
    processNext();
  }
}
