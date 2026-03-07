#include "sessionswindow.h"
#include "apimanager.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"

#include <KLocalizedString>
#include <QAction>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <KActionCollection>

SessionsWindow::SessionsWindow(const QString &filterSource,
                               APIManager *apiManager, QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(apiManager),
      m_filterSource(filterSource), m_sessionsLoaded(0), m_isRefreshing(false) {

  m_model = new SessionModel(this);
  m_proxyModel = new QSortFilterProxyModel(this);
  m_proxyModel->setSourceModel(m_model);

  if (!m_filterSource.isEmpty()) {
    m_proxyModel->setFilterRole(SessionModel::SourceRole);
    m_proxyModel->setFilterFixedString(m_filterSource);
    setWindowTitle(i18n("Sessions for %1", m_filterSource));
  } else {
    setWindowTitle(i18n("All Sessions"));
    m_model->loadSessions();
  }

  setupUi();

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionsReceived, this,
            &SessionsWindow::onSessionsReceived);
    connect(m_apiManager, &APIManager::sessionsRefreshFinished, this,
            &SessionsWindow::onSessionsRefreshFinished);
  }

  if (m_model->rowCount() == 0 || !m_filterSource.isEmpty()) {
    refreshSessions();
  } else {
    m_statusLabel->setText(
        i18n("Loaded %1 cached sessions.", m_model->rowCount()));
  }
}

SessionsWindow::~SessionsWindow() {}

void SessionsWindow::setupUi() {
  setAttribute(Qt::WA_DeleteOnClose);
  resize(800, 600);

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);
  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  QTabWidget *tabWidget = new QTabWidget(this);

  m_listView = new QListView(this);
  m_listView->setModel(m_proxyModel);
  m_listView->setItemDelegate(new SessionDelegate(this));
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);

  tabWidget->addTab(m_listView, i18n("All sessions"));

  connect(
      m_listView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_listView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *openUrlAction = menu.addAction(i18n("Open URL"));
          QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

          connect(openUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_proxyModel->data(index, SessionModel::IdRole).toString();
            m_statusLabel->setText(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_proxyModel->data(index, SessionModel::IdRole).toString();
            QGuiApplication::clipboard()->setText(
                QStringLiteral("https://jules.google.com/sessions/") + id);
            m_statusLabel->setText(i18n("URL copied to clipboard."));
          });
          menu.exec(m_listView->mapToGlobal(pos));
        }
      });

  connect(m_listView, &QListView::doubleClicked, this,
          [this](const QModelIndex &index) {
            QString id =
                m_proxyModel->data(index, SessionModel::IdRole).toString();
            if (m_apiManager) {
              m_statusLabel->setText(
                  i18n("Fetching details for session %1...", id));
              m_apiManager->getSession(id);
            }
          });

  layout->addWidget(tabWidget);

  // Actions
  QAction *refreshAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
  connect(refreshAction, &QAction::triggered, this, &SessionsWindow::refreshSessions);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"), refreshAction);
  actionCollection()->setDefaultShortcut(refreshAction, QKeySequence(Qt::Key_F5));

  // Menu
  QMenu *fileMenu = new QMenu(i18n("File"), this);
  fileMenu->addAction(refreshAction);
  QAction *quitAction = new QAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Close"), this);
  connect(quitAction, &QAction::triggered, this, &SessionsWindow::close);
  fileMenu->addAction(quitAction);
  menuBar()->addMenu(fileMenu);

  // Toolbar
  QToolBar *toolBar = addToolBar(i18n("Main Toolbar"));
  toolBar->setObjectName(QStringLiteral("mainToolBar"));
  toolBar->addAction(refreshAction);

  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setMinimum(0);
  m_progressBar->setMaximum(0);
  m_progressBar->hide();
  statusBar()->addPermanentWidget(m_progressBar);

  m_cancelBtn = new QPushButton(i18n("Cancel"), this);
  m_cancelBtn->hide();
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &SessionsWindow::cancelRefresh);
  statusBar()->addPermanentWidget(m_cancelBtn);
}

void SessionsWindow::refreshSessions() {
  if (m_isRefreshing) {
    cancelRefresh();
    return;
  }
  if (!m_apiManager)
    return;

  m_isRefreshing = true;
  m_sessionsLoaded = 0;

  if (m_filterSource.isEmpty()) {
    // If we're getting the global list, clear the current model
    m_model->clearSessions();
  } else {
    // We don't necessarily clear it if it's a filtered view. We can keep
    // pulling and append, but clearing might be better to avoid dupes since the
    // pagination doesn't allow filtering server-side based on the source easily
    // yet. But the API says `GET /sessions`. We just pull all and filter. So
    // let's clear it.
    m_model->clearSessions();
  }

  m_progressBar->show();
  m_cancelBtn->show();
  m_statusLabel->setText(i18n("Refreshing sessions..."));
  m_apiManager->listSessions();
}

void SessionsWindow::cancelRefresh() {
  if (m_apiManager) {
    m_apiManager->cancelListSessions();
  }
  m_isRefreshing = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(i18n("Refresh cancelled. Loaded %1 sessions.", m_sessionsLoaded));
}

void SessionsWindow::onSessionsReceived(const QJsonArray &sessions) {
  int added = m_model->addSessions(sessions);
  m_sessionsLoaded += added;
  m_progressBar->setFormat(i18n("%1 sessions loaded", m_sessionsLoaded));
  m_statusLabel->setText(i18n("Loaded %1 sessions...", m_sessionsLoaded));
}

void SessionsWindow::onSessionsRefreshFinished() {
  m_isRefreshing = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(
      i18n("Finished refreshing. Loaded %1 sessions.", m_sessionsLoaded));
  if (m_filterSource.isEmpty()) {
    m_model->saveSessions();
  }
}
