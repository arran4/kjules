#include "refreshprogresswindow.h"
#include "apimanager.h"

#include <QVBoxLayout>
#include <QProgressBar>
#include <QTextBrowser>
#include <QPushButton>
#include <KI18n/KLocalizedString>

RefreshProgressWindow::RefreshProgressWindow(const QStringList &sessionIds, APIManager *apiManager, QWidget *parent)
    : QDialog(parent),
      m_queue(sessionIds),
      m_totalCount(sessionIds.size()),
      m_currentIndex(0),
      m_apiManager(apiManager)
{
    setWindowTitle(i18n("Refresh Progress"));
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

    connect(m_apiManager, &APIManager::sessionReloaded, this, &RefreshProgressWindow::onSessionReloaded);
    connect(m_apiManager, &APIManager::errorOccurred, this, &RefreshProgressWindow::onErrorOccurred);

    // Start processing asynchronously so the UI can show up
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

RefreshProgressWindow::~RefreshProgressWindow()
{
}

void RefreshProgressWindow::processNext()
{
    if (!m_apiManager->canConnect()) {
        m_textBrowser->append(i18n("<b>Error:</b> Cannot refresh: No token or previous failure. Processing stopped."));
        m_progressBar->setValue(m_totalCount); // Force completion
        m_closeButton->setEnabled(true);
        return;
    }

    if (m_currentIndex >= m_totalCount) {
        m_textBrowser->append(i18n("<b>Finished.</b>"));
        m_closeButton->setEnabled(true);
        return;
    }

    QString id = m_queue[m_currentIndex];
    m_textBrowser->append(i18n("Refreshing session %1...", id));
    m_apiManager->reloadSession(id);
}

void RefreshProgressWindow::onSessionReloaded(const QJsonObject &session)
{
    QString id = session.value(QStringLiteral("id")).toString();
    // Verify it's the one we are waiting for just in case
    if (m_currentIndex < m_totalCount && id == m_queue[m_currentIndex]) {
        m_textBrowser->append(i18n("<font color='green'>Successfully reloaded session %1.</font>", id));
        m_currentIndex++;
        m_progressBar->setValue(m_currentIndex);
        processNext();
    }
}

void RefreshProgressWindow::onErrorOccurred(const QString &message)
{
    // The APIManager emits this for any error. We assume it correlates to the current item being processed.
    if (m_currentIndex < m_totalCount) {
        QString id = m_queue[m_currentIndex];
        m_textBrowser->append(i18n("<font color='red'>Failed to reload session %1: %2</font>", id, message));
        m_currentIndex++;
        m_progressBar->setValue(m_currentIndex);
        processNext();
    }
}
