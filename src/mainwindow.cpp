#include "mainwindow.h"
#include "apimanager.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "newsessiondialog.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"
#include "settingsdialog.h"
#include "sourcemodel.h"
#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
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
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(new APIManager(this)),
      m_sessionModel(new SessionModel(this)),
      m_sourceModel(new SourceModel(this)),
      m_draftsModel(new DraftsModel(this)) {
  setupUi();
  setupTrayIcon();
  createActions();

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sourcesReceived, m_sourceModel,
          &SourceModel::setSources);
  connect(m_apiManager, &APIManager::sessionsReceived, m_sessionModel,
          &SessionModel::setSessions);
  connect(m_apiManager, &APIManager::sessionCreated,
          [this](const QJsonObject &session) {
            m_sessionModel->addSession(session);
            updateStatus(
                i18n("Session created: %1",
                     session.value(QStringLiteral("title")).toString()));
          });
  connect(m_apiManager, &APIManager::sessionDetailsReceived, this,
          &MainWindow::showSessionWindow);
  connect(m_apiManager, &APIManager::errorOccurred, this, &MainWindow::onError);
  connect(m_apiManager, &APIManager::logMessage, this,
          &MainWindow::updateStatus);

  // Initial refresh
  QTimer::singleShot(0, this, [this]() {
    refreshSources();
    refreshSessions();
  });
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  QTabWidget *tabWidget = new QTabWidget(this);

  // Sources View
  m_sourceView = new QListView(this);
  m_sourceView->setModel(m_sourceModel);
  connect(m_sourceView, &QListView::doubleClicked, this,
          &MainWindow::onSourceActivated);
  tabWidget->addTab(m_sourceView, i18n("Sources"));

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
            // Placeholder URL logic
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();
            // Placeholder URL logic
            QGuiApplication::clipboard()->setText(
                QStringLiteral("https://jules.google.com/sessions/") + id);
            updateStatus(i18n("URL copied to clipboard."));
          });
          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QListView::doubleClicked, this,
          &MainWindow::onSessionActivated);

  tabWidget->addTab(m_sessionView, i18n("Past"));

  // Drafts View
  m_draftsView = new QListView(this);
  m_draftsView->setModel(m_draftsModel);
  m_draftsView->setItemDelegate(new DraftDelegate(this));
  m_draftsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_draftsView, &QListView::customContextMenuRequested,
          [this](const QPoint &pos) {
            QModelIndex index = m_draftsView->indexAt(pos);
            if (index.isValid()) {
              QMenu menu;
              QAction *submitAction = menu.addAction(i18n("Submit Now"));
              QAction *duplicateAction = menu.addAction(i18n("Duplicate"));
              QAction *deleteAction = menu.addAction(i18n("Delete"));

              connect(submitAction, &QAction::triggered,
                      [this, index]() { onDraftActivated(index); });

              connect(duplicateAction, &QAction::triggered, [this, index]() {
                QJsonObject draft = m_draftsModel->getDraft(index.row());
                m_draftsModel->addDraft(draft);
                updateStatus(i18n("Draft duplicated."));
              });

              connect(deleteAction, &QAction::triggered, [this, index]() {
                if (QMessageBox::question(this, i18n("Delete Draft"),
                                          i18n("Are you sure?")) ==
                    QMessageBox::Yes) {
                  m_draftsModel->removeDraft(index.row());
                }
              });

              menu.exec(m_draftsView->mapToGlobal(pos));
            }
          });
  connect(m_draftsView, &QListView::doubleClicked, this,
          &MainWindow::onDraftActivated);

  tabWidget->addTab(m_draftsView, i18n("Drafts"));

  mainLayout->addWidget(tabWidget);

  // Toolbar / Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *refreshSourcesBtn =
      new QPushButton(i18n("Refresh Sources"), this);
  connect(refreshSourcesBtn, &QPushButton::clicked, this,
          &MainWindow::refreshSources);

  QPushButton *refreshSessionsBtn =
      new QPushButton(i18n("Refresh Sessions"), this);
  connect(refreshSessionsBtn, &QPushButton::clicked, this,
          &MainWindow::refreshSessions);

  QPushButton *newSessionButton = new QPushButton(i18n("New Session"), this);
  connect(newSessionButton, &QPushButton::clicked, this,
          &MainWindow::showNewSessionDialog);

  QPushButton *settingsButton = new QPushButton(i18n("Settings"), this);
  connect(settingsButton, &QPushButton::clicked, this,
          &MainWindow::showSettingsDialog);

  buttonLayout->addWidget(refreshSourcesBtn);
  buttonLayout->addWidget(refreshSessionsBtn);
  buttonLayout->addWidget(settingsButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(newSessionButton);

  mainLayout->addLayout(buttonLayout);

  // Status Bar
  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new KStatusNotifierItem(this);
  m_trayIcon->setIconByName(QStringLiteral("sc-apps-kjules"));
  m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
  m_trayIcon->setStatus(KStatusNotifierItem::Active);
  m_trayIcon->setToolTip(QStringLiteral("sc-apps-kjules"), i18n("kJules"),
                         i18n("Google Jules Client"));

  QMenu *menu = m_trayIcon->contextMenu();
  QAction *newSessionAction = menu->addAction(i18n("New Session"));
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);

  connect(m_trayIcon, &KStatusNotifierItem::activateRequested, this,
          &MainWindow::toggleWindow);
}

void MainWindow::createActions() {
  QAction *newSessionAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-new")),
                  i18n("New Session"), this);
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);
  actionCollection()->addAction(QStringLiteral("new_session"),
                                newSessionAction);
  KGlobalAccel::setGlobalShortcut(newSessionAction,
                                  QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_N));

  QAction *refreshSourcesAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sources"), this);
  connect(refreshSourcesAction, &QAction::triggered, this,
          &MainWindow::refreshSources);
  actionCollection()->addAction(QStringLiteral("refresh_sources"),
                                refreshSourcesAction);

  QAction *refreshSessionsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sessions"), this);
  connect(refreshSessionsAction, &QAction::triggered, this,
          &MainWindow::refreshSessions);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"),
                                refreshSessionsAction);

  KStandardAction::preferences(this, &MainWindow::showSettingsDialog,
                               actionCollection());
  KStandardAction::quit(qApp, &QCoreApplication::quit, actionCollection());

  setupGUI(Default, QStringLiteral("kjulesui.rc"));
}

void MainWindow::refreshSources() {
  updateStatus(i18n("Refreshing sources..."));
  m_apiManager->listSources();
}

void MainWindow::refreshSessions() {
  updateStatus(i18n("Refreshing sessions..."));
  m_apiManager->listSessions();
}

void MainWindow::showNewSessionDialog() {
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
  connect(&dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  dialog.exec();
}

void MainWindow::showSettingsDialog() {
  SettingsDialog dialog(m_apiManager, this);
  dialog.exec();
}

void MainWindow::onSessionCreated(const QStringList &sources,
                                  const QString &prompt,
                                  const QString &automationMode) {
  for (const QString &source : sources) {
    m_apiManager->createSession(source, prompt, automationMode);
  }
}

void MainWindow::onDraftSaved(const QJsonObject &draft) {
  m_draftsModel->addDraft(draft);
  updateStatus(i18n("Draft saved."));
}

void MainWindow::onDraftActivated(const QModelIndex &index) {
  QJsonObject draft = m_draftsModel->getDraft(index.row());
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
  dialog.setInitialData(draft);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, index](const QStringList &sources, const QString &p,
                        const QString &a) {
            onSessionCreated(sources, p, a);
            m_draftsModel->removeDraft(index.row());
          });

  connect(&dialog, &NewSessionDialog::saveDraftRequested,
          [this, index](const QJsonObject &d) {
            m_draftsModel->removeDraft(index.row());
            m_draftsModel->addDraft(d);
            updateStatus(i18n("Draft updated."));
          });

  dialog.exec();
}

void MainWindow::onSourceActivated(const QModelIndex &index) {
  QString sourceName =
      m_sourceModel->data(index, SourceModel::NameRole).toString();
  QJsonObject initData;
  initData[QStringLiteral("source")] = sourceName;

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
  dialog.setInitialData(initData);

  connect(&dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  dialog.exec();
}

void MainWindow::showSessionWindow(const QJsonObject &session) {
  SessionWindow *window = new SessionWindow(session, this);
  window->show();
}

void MainWindow::onSessionActivated(const QModelIndex &index) {
  QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();
  m_apiManager->getSession(id);
  updateStatus(i18n("Fetching details for session %1...", id));
}

void MainWindow::updateStatus(const QString &message) {
  m_statusLabel->setText(message);
  m_trayIcon->setToolTip(QStringLiteral("sc-apps-kjules"), i18n("kJules"),
                         message);
}

void MainWindow::onError(const QString &message) {
  updateStatus(i18n("Error: %1", message));
  QMessageBox::critical(this, i18n("Error"), message);
}

void MainWindow::toggleWindow() {
  if (isVisible()) {
    hide();
  } else {
    show();
    raise();
    activateWindow();
  }
}
