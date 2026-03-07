#include "mainwindow.h"
#include "apimanager.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "newsessiondialog.h"
#include "queuedelegate.h"
#include "queuemodel.h"
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
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
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
      m_draftsModel(new DraftsModel(this)), m_queueModel(new QueueModel(this)),
      m_isRefreshingSources(false), m_sourcesLoadedCount(0),
      m_sourcesAddedCount(0), m_pagesLoadedCount(0),
      m_sessionRefreshTimer(new QTimer(this)), m_queueTimer(new QTimer(this)),
      m_isProcessingQueue(false) {
  setupUi();

  connect(m_sessionRefreshTimer, &QTimer::timeout, this,
          &MainWindow::updateSessionStats);
  m_sessionRefreshTimer->start(60000); // 1 minute

  connect(m_queueTimer, &QTimer::timeout, this, &MainWindow::processQueue);
  if (!m_queueModel->isEmpty()) {
    m_queueTimer->start(60000); // 1 minute
  }
  setupTrayIcon();
  createActions();

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sourcesReceived, this,
          &MainWindow::onSourcesReceived);
  connect(m_apiManager, &APIManager::sourcesRefreshFinished, this,
          &MainWindow::onSourcesRefreshFinished);
  connect(m_apiManager, &APIManager::sessionsReceived, this,
          [this](const QJsonArray &sessions) {
            m_sessionModel->setSessions(sessions);
            m_lastSessionRefreshTime = QDateTime::currentDateTime();
            updateSessionStats();
          });
  connect(m_apiManager, &APIManager::sessionCreated,
          [this](const QJsonObject &session) {
            onSessionCreatedResult(true, session, QString());
          });
  connect(m_apiManager, &APIManager::sessionDetailsReceived, this,
          &MainWindow::showSessionWindow);
  connect(m_apiManager, &APIManager::errorOccurred, this,
          [this](const QString &msg) {
            if (m_isProcessingQueue) {
              onSessionCreatedResult(false, QJsonObject(), msg);
            } else {
              onError(msg);
            }
          });
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
  m_sourceView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_sourceView, &QListView::customContextMenuRequested,
          [this](const QPoint &pos) {
            QModelIndex index = m_sourceView->indexAt(pos);
            if (index.isValid()) {
              QString id = index.data(SourceModel::IdRole).toString();
              QString urlStr;
              QStringList parts = id.split(QLatin1Char('/'));
              if (parts.size() >= 4 && parts[0] == QStringLiteral("sources")) {
                QString provider = parts[1];
                QString owner = parts[2];
                QString repo = parts[3];
                if (provider == QStringLiteral("github")) {
                  urlStr = QStringLiteral("https://github.com/") + owner +
                           QLatin1Char('/') + repo;
                } else if (provider == QStringLiteral("gitlab")) {
                  urlStr = QStringLiteral("https://gitlab.com/") + owner +
                           QLatin1Char('/') + repo;
                } else if (provider == QStringLiteral("bitbucket")) {
                  urlStr = QStringLiteral("https://bitbucket.org/") + owner +
                           QLatin1Char('/') + repo;
                } else {
                  urlStr = QStringLiteral("https://") + provider +
                           QStringLiteral(".com/") + owner + QLatin1Char('/') +
                           repo;
                }
              } else {
                urlStr = id;
              }

              QMenu menu;
              QAction *openUrlAction = menu.addAction(i18n("Open URL"));
              QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

              connect(openUrlAction, &QAction::triggered, [this, id, urlStr]() {
                QDesktopServices::openUrl(QUrl(urlStr));
                updateStatus(i18n("Opening source %1", id));
              });

              connect(copyUrlAction, &QAction::triggered, [this, urlStr]() {
                QGuiApplication::clipboard()->setText(urlStr);
                updateStatus(i18n("URL copied to clipboard."));
              });
              menu.exec(m_sourceView->mapToGlobal(pos));
            }
          });
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

            QRegularExpression idRegex(QStringLiteral("^[a-zA-Z0-9_-]+$"));
            if (!idRegex.match(id).hasMatch()) {
              updateStatus(i18n("Invalid session ID format."));
              return;
            }

            // Placeholder URL logic
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();

            QRegularExpression idRegex(QStringLiteral("^[a-zA-Z0-9_-]+$"));
            if (!idRegex.match(id).hasMatch()) {
              updateStatus(i18n("Invalid session ID format."));
              return;
            }

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

  // Queue View
  m_queueView = new QListView(this);
  m_queueView->setModel(m_queueModel);
  m_queueView->setItemDelegate(new QueueDelegate(this));
  m_queueView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_queueView, &QListView::activated, this,
          &MainWindow::onQueueActivated);
  connect(m_queueView, &QListView::customContextMenuRequested, this,
          &MainWindow::onQueueContextMenu);
  tabWidget->addTab(m_queueView, i18n("Queue"));

  mainLayout->addWidget(tabWidget);

  // Toolbar / Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  m_refreshSourcesBtn = new QPushButton(i18n("Refresh Sources"), this);
  connect(m_refreshSourcesBtn, &QPushButton::clicked, this,
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

  buttonLayout->addWidget(m_refreshSourcesBtn);
  buttonLayout->addWidget(refreshSessionsBtn);
  buttonLayout->addWidget(settingsButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(newSessionButton);

  mainLayout->addLayout(buttonLayout);

  // Status Bar
  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);

  m_sessionStatsLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_sessionStatsLabel);
  updateSessionStats();

  m_sourceProgressBar = new QProgressBar(this);
  m_sourceProgressBar->setMinimum(0);
  m_sourceProgressBar->setMaximum(0); // Indeterminate
  m_sourceProgressBar->hide();
  statusBar()->addPermanentWidget(m_sourceProgressBar);

  m_cancelRefreshBtn = new QPushButton(i18n("Cancel"), this);
  m_cancelRefreshBtn->hide();
  connect(m_cancelRefreshBtn, &QPushButton::clicked, this,
          &MainWindow::cancelSourcesRefresh);
  statusBar()->addPermanentWidget(m_cancelRefreshBtn);
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

  m_refreshSourcesAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sources"), this);
  connect(m_refreshSourcesAction, &QAction::triggered, this,
          &MainWindow::refreshSources);
  actionCollection()->addAction(QStringLiteral("refresh_sources"),
                                m_refreshSourcesAction);

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
  if (m_isRefreshingSources) {
    cancelSourcesRefresh();
    return;
  }
  m_isRefreshingSources = true;
  m_sourcesLoadedCount = 0;
  m_sourcesAddedCount = 0;
  m_pagesLoadedCount = 0;

  m_refreshSourcesBtn->setText(i18n("Cancel Refresh"));
  m_refreshSourcesAction->setText(i18n("Cancel Refresh"));
  m_sourceProgressBar->show();
  m_cancelRefreshBtn->show();

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
    QJsonObject req;
    req[QStringLiteral("source")] = source;
    req[QStringLiteral("prompt")] = prompt;
    if (!automationMode.isEmpty()) {
      req[QStringLiteral("automationMode")] = automationMode;
    }
    m_queueModel->enqueue(req);
  }
  updateStatus(i18np("Added 1 task to queue.", "Added %1 tasks to queue.",
                     sources.size()));

  // Start timer if not running
  if (!m_queueTimer->isActive()) {
    m_queueTimer->start(60000); // 1 minute
  }

  // Trigger processing immediately if we can
  QTimer::singleShot(0, this, &MainWindow::processQueue);
}

void MainWindow::processQueue() {
  if (m_isProcessingQueue)
    return;
  if (m_queueModel->isEmpty()) {
    m_queueTimer->stop();
    return;
  }

  if (m_queueBackoffUntil.isValid() &&
      QDateTime::currentDateTimeUtc() < m_queueBackoffUntil) {
    // We are backing off
    return;
  }

  m_isProcessingQueue = true;
  QueueItem item = m_queueModel->peek();
  m_apiManager->createSessionAsync(item.requestData);
}

void MainWindow::onSessionCreatedResult(bool success,
                                        const QJsonObject &session,
                                        const QString &errorMsg) {
  if (!m_isProcessingQueue) {
    if (success) {
      m_sessionModel->addSession(session);
      updateStatus(i18n("Session created successfully."));
    }
    return;
  }

  QueueItem item = m_queueModel->dequeue(); // Pop the item we were processing
  m_isProcessingQueue = false;

  if (success) {
    m_sessionModel->addSession(session);
    updateStatus(i18n("Session created from queue."));
    m_queueBackoffUntil = QDateTime(); // reset backoff
    // The next item will be processed by the 1-minute timer (m_queueTimer)
  } else {
    updateStatus(i18n("Failed to create session from queue: %1", errorMsg));
    m_queueModel->requeueFailed(item, errorMsg);
    m_queueBackoffUntil =
        QDateTime::currentDateTimeUtc().addSecs(30 * 60); // 30 minutes backoff
  }
}

void MainWindow::onDraftSaved(const QJsonObject &draft) {
  m_draftsModel->addDraft(draft);
  updateStatus(i18n("Draft saved."));
}

void MainWindow::onQueueActivated(const QModelIndex &index) {
  if (!index.isValid())
    return;
  editQueueItem(index.row());
}

void MainWindow::onQueueContextMenu(const QPoint &pos) {
  QModelIndex index = m_queueView->indexAt(pos);
  if (!index.isValid())
    return;
  int row = index.row();

  QMenu menu;
  QAction *editAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit"));
  QAction *deleteAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));
  QAction *draftAction =
      menu.addAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                     i18n("Convert to Draft"));
  QAction *sendAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("mail-send")), i18n("Send Now"));

  QAction *selected = menu.exec(m_queueView->viewport()->mapToGlobal(pos));
  if (selected == editAction) {
    editQueueItem(row);
  } else if (selected == deleteAction) {
    if (QMessageBox::question(this, i18n("Remove Task"),
                              i18n("Remove this task from the queue?")) ==
        QMessageBox::Yes) {
      m_queueModel->removeItem(row);
      updateStatus(i18n("Task removed from queue."));
    }
  } else if (selected == draftAction) {
    convertQueueItemToDraft(row);
  } else if (selected == sendAction) {
    sendQueueItemNow(row);
  }
}

void MainWindow::sendQueueItemNow(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.requestData.isEmpty())
    return;
  m_queueModel->removeItem(row);
  updateStatus(i18n("Sending queue item immediately..."));
  m_apiManager->createSessionAsync(item.requestData);
}

void MainWindow::editQueueItem(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.requestData.isEmpty())
    return;

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
  dialog.setEditMode(true);
  dialog.setInitialData(item.requestData);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, row](const QStringList &sources, const QString &p,
                      const QString &a) {
            m_queueModel->removeItem(row);
            onSessionCreated(sources, p, a);
          });

  connect(&dialog, &NewSessionDialog::saveDraftRequested,
          [this, row](const QJsonObject &d) {
            m_queueModel->removeItem(row);
            m_draftsModel->addDraft(d);
            updateStatus(i18n("Task moved to drafts."));
          });

  dialog.exec();
}

void MainWindow::convertQueueItemToDraft(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.requestData.isEmpty())
    return;

  m_queueModel->removeItem(row);
  m_draftsModel->addDraft(item.requestData);
  updateStatus(i18n("Task converted to draft."));
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

void MainWindow::onSourcesReceived(const QJsonArray &sources) {
  int added = m_sourceModel->addSources(sources);
  m_sourcesLoadedCount += sources.size();
  m_sourcesAddedCount += added;
  m_pagesLoadedCount++;

  m_sourceProgressBar->setFormat(i18n("%1 sources loaded from %2 pages",
                                      m_sourcesLoadedCount,
                                      m_pagesLoadedCount));
  updateStatus(i18n("Loaded %1 sources, added %2 new.", m_sourcesLoadedCount,
                    m_sourcesAddedCount));
}

void MainWindow::onSourcesRefreshFinished() {
  bool wasRefreshing = m_isRefreshingSources;
  m_isRefreshingSources = false;
  m_sourceProgressBar->hide();
  m_cancelRefreshBtn->hide();
  m_refreshSourcesBtn->setText(i18n("Refresh Sources"));
  m_refreshSourcesAction->setText(i18n("Refresh Sources"));

  if (wasRefreshing) {
    updateStatus(
        i18n("Finished refreshing. Loaded %1 sources in total, %2 new.",
             m_sourcesLoadedCount, m_sourcesAddedCount));
  } else {
    updateStatus(i18n("Source refresh cancelled. Loaded %1 sources, %2 new.",
                      m_sourcesLoadedCount, m_sourcesAddedCount));
  }
}

void MainWindow::cancelSourcesRefresh() {
  m_apiManager->cancelListSources();
  m_isRefreshingSources = false;
  m_sourceProgressBar->hide();
  m_cancelRefreshBtn->hide();
  m_refreshSourcesBtn->setText(i18n("Refresh Sources"));
  m_refreshSourcesAction->setText(i18n("Refresh Sources"));
  // Omit updateStatus here because APIManager's cancelation will
  // subsequently emit sourcesRefreshFinished(), which would clobber this.
  // Instead, we let onSourcesRefreshFinished() handle the final status text.
}

void MainWindow::updateSessionStats() {
  int sessionCount = m_sessionModel->rowCount();
  QString timeStr = i18n("Never");

  if (m_lastSessionRefreshTime.isValid()) {
    qint64 secs = m_lastSessionRefreshTime.secsTo(QDateTime::currentDateTime());
    if (secs < 60) {
      timeStr = i18n("Just now");
    } else {
      qint64 mins = secs / 60;
      timeStr = i18np("1 min ago", "%1 mins ago", mins);
    }
  }

  m_sessionStatsLabel->setText(
      i18n("Sessions: %1 | Updated: %2", sessionCount, timeStr));
}
