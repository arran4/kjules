#include "sourcesessionswindow.h"
#include "apimanager.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"
#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KNotification>
#include <KStandardAction>
#include <KStatusNotifierItem>
#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

SourceSessionsWindow::SourceSessionsWindow(const QString &sourceName,
                                           QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(new APIManager(this)),
      m_sessionModel(new SessionModel(this)), m_sourceName(sourceName) {

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sessionsReceived,
          [this](const QJsonArray &sessions) {
            QJsonArray filteredSessions;
            for (const auto &s : sessions) {
              QJsonObject obj = s.toObject();
              QString src = obj.value(QStringLiteral("sourceContext"))
                                .toObject()
                                .value(QStringLiteral("source"))
                                .toString();
              if (src == m_sourceName) {
                filteredSessions.append(s);
              }
            }
            m_sessionModel->setSessions(filteredSessions);
          });

  connect(m_apiManager, &APIManager::sessionDetailsReceived, this,
          &SourceSessionsWindow::showSessionWindow);
  connect(m_apiManager, &APIManager::errorOccurred, this,
          &SourceSessionsWindow::onError);
  connect(m_apiManager, &APIManager::logMessage, this,
          [this](const QString &message) { qDebug() << message; });

  m_apiManager->loadApiKeyFromWallet();
  m_apiManager->loadGithubTokenFromWallet();

  setupUi();
  setupTrayIcon();
  createActions();

  // Initial load
  QTimer::singleShot(0, this, &SourceSessionsWindow::refreshSessions);
}

SourceSessionsWindow::~SourceSessionsWindow() {}

void SourceSessionsWindow::setupUi() {
  setWindowTitle(i18n("Sessions for %1", m_sourceName));
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  QTabWidget *tabWidget = new QTabWidget(this);

  // Sessions View
  m_sessionView = new QListView(this);
  m_sessionView->setModel(m_sessionModel);
  m_sessionView->setItemDelegate(new SessionDelegate(this));
  m_sessionView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sessionView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *openUrlAction = menu.addAction(i18n("Open URL"));
          QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

          connect(openUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();
            QGuiApplication::clipboard()->setText(
                QStringLiteral("https://jules.google.com/sessions/") + id);
            updateStatus(i18n("URL copied to clipboard."));
          });
          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QListView::doubleClicked, this,
          &SourceSessionsWindow::onSessionActivated);

  tabWidget->addTab(m_sessionView, i18n("Sessions"));

  mainLayout->addWidget(tabWidget);

  // Status Bar
  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);
}

void SourceSessionsWindow::setupTrayIcon() {
  m_trayIcon = new KStatusNotifierItem(this);
  m_trayIcon->setIconByName(QStringLiteral("sc-apps-kjules"));
  m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
  m_trayIcon->setStatus(KStatusNotifierItem::Active);
  m_trayIcon->setToolTip(QStringLiteral("sc-apps-kjules"),
                         i18n("kJules: %1", m_sourceName),
                         i18n("Google Jules Client"));

  connect(m_trayIcon, &KStatusNotifierItem::activateRequested, this,
          &SourceSessionsWindow::toggleWindow);
}

void SourceSessionsWindow::createActions() {
  QAction *refreshSessionsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sessions"), this);
  connect(refreshSessionsAction, &QAction::triggered, this,
          &SourceSessionsWindow::refreshSessions);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"),
                                refreshSessionsAction);

  setupGUI(Default, QStringLiteral("kjulesui.rc"));
}

void SourceSessionsWindow::refreshSessions() {
  updateStatus(i18n("Refreshing sessions..."));
  m_apiManager->listSessions();
}

void SourceSessionsWindow::showSessionWindow(const QJsonObject &session) {
  SessionWindow *window = new SessionWindow(session, this);
  window->show();
}

void SourceSessionsWindow::onSessionActivated(const QModelIndex &index) {
  QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();
  m_apiManager->getSession(id);
  updateStatus(i18n("Fetching details for session %1...", id));
}

void SourceSessionsWindow::updateStatus(const QString &message) {
  m_statusLabel->setText(message);
  m_trayIcon->setToolTip(QStringLiteral("sc-apps-kjules"),
                         i18n("kJules: %1", m_sourceName), message);

  if (message.contains(i18n("Refreshing")) ||
      message.contains(i18n("Fetching"))) {
    KNotification *notification =
        new KNotification(QStringLiteral("notification"));
    notification->setTitle(i18n("kJules: %1", m_sourceName));
    notification->setText(message);
    notification->setIconName(QStringLiteral("sc-apps-kjules"));
    notification->sendEvent();
  }
}

void SourceSessionsWindow::onError(const QString &message) {
  updateStatus(i18n("Error: %1", message));
  QMessageBox::critical(this, i18n("Error"), message);
}

void SourceSessionsWindow::toggleWindow() {
  if (isVisible()) {
    hide();
  } else {
    show();
    raise();
    activateWindow();
  }
}
