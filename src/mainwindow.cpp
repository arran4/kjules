#include "mainwindow.h"
#include "advancedfilterproxymodel.h"
#include "apimanager.h"
#include "backupdialog.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "errorsmodel.h"
#include "errorwindow.h"
#include "filtereditor.h"
#include "newsessiondialog.h"
#include "queuedelegate.h"
#include "queuemodel.h"
#include "restoredialog.h"
#include "savedialog.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"
#include "settingsdialog.h"
#include "sourcemodel.h"
#include "templateeditdialog.h"
#include "templatesmodel.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToolBar>
#include <KZip>
#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(new APIManager(this)),
      m_sessionModel(
          new SessionModel(QStringLiteral("cached_all_sessions.json"), this)),
      m_archiveModel(new SessionModel(
          QStringLiteral("cached_archive_sessions.json"), this)),
      m_sourceModel(new SourceModel(this)),
      m_draftsModel(new DraftsModel(this)),
      m_templatesModel(new TemplatesModel(this)),
      m_queueModel(new QueueModel(this)), m_errorsModel(new ErrorsModel(this)),
      m_isRefreshingSources(false), m_sourcesLoadedCount(0),
      m_sourcesAddedCount(0), m_pagesLoadedCount(0),
      m_sessionRefreshTimer(new QTimer(this)), m_queueTimer(new QTimer(this)),
      m_isProcessingQueue(false), m_queuePaused(false) {
  setObjectName(QStringLiteral("MainWindow"));
  setupUi();

  connect(m_sessionRefreshTimer, &QTimer::timeout, this,
          &MainWindow::updateSessionStats);
  m_sessionRefreshTimer->start(60000); // 1 minute

  connect(m_queueTimer, &QTimer::timeout, this, &MainWindow::processQueue);
  loadQueueSettings();
  createActions();
  setupTrayIcon();

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sourcesReceived, this,
          &MainWindow::onSourcesReceived);
  connect(m_apiManager, &APIManager::sourcesRefreshFinished, this,
          &MainWindow::onSourcesRefreshFinished);
  connect(m_apiManager, &APIManager::githubInfoReceived, this,
          &MainWindow::onGithubInfoReceived);
  connect(m_apiManager, &APIManager::githubPullRequestInfoReceived, this,
          &MainWindow::onGithubPullRequestInfoReceived);
  connect(m_apiManager, &APIManager::sessionsReceived, this,
          [this](const QJsonArray &sessions) {
            m_sessionModel->setSessions(sessions);
            m_lastSessionRefreshTime = QDateTime::currentDateTime();
            updateSessionStats();
            for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
              QString prUrl = m_sessionModel
                                  ->data(m_sessionModel->index(i, 0),
                                         SessionModel::PrUrlRole)
                                  .toString();
              if (!prUrl.isEmpty()) {
                m_apiManager->fetchGithubPullRequest(prUrl);
              }
            }
          });
  connect(m_apiManager, &APIManager::sessionCreationFailed, this,
          &MainWindow::onSessionCreationFailed);
  connect(m_apiManager, &APIManager::sessionCreated,
          [this](const QJsonObject &session) {
            onSessionCreatedResult(true, session, QString());
          });
  connect(m_apiManager, &APIManager::sessionDetailsReceived, this,
          &MainWindow::showSessionWindow);
  connect(m_apiManager, &APIManager::sessionReloaded, this,
          [this](const QJsonObject &session) {
            m_sessionModel->updateSession(session);
            // We need to fetch github PR info if we have one
            for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
              if (m_sessionModel
                      ->data(m_sessionModel->index(i, 0), SessionModel::IdRole)
                      .toString() ==
                  session.value(QStringLiteral("id")).toString()) {
                QString prUrl = m_sessionModel
                                    ->data(m_sessionModel->index(i, 0),
                                           SessionModel::PrUrlRole)
                                    .toString();
                if (!prUrl.isEmpty()) {
                  m_apiManager->fetchGithubPullRequest(prUrl);
                }
                break;
              }
            }
          });
  connect(m_apiManager, &APIManager::sourceDetailsReceived, this,
          &MainWindow::onSourceDetailsReceived);
  connect(m_apiManager, &APIManager::errorOccurred, this,
          [this](const QString &msg) {
            if (!m_isProcessingQueue) {
              onError(msg);
            }
            // For queue errors we rely on errorOccurredWithResponse
          });
  connect(m_apiManager, &APIManager::errorOccurredWithResponse, this,
          [this](const QString &msg, const QString &response) {
            if (m_isProcessingQueue) {
              onSessionCreatedResult(false, QJsonObject(), msg, response);
            }
          });
  connect(m_apiManager, &APIManager::logMessage, this,
          &MainWindow::updateStatus);

  auto updateSourceStats = [this]() {
    QJsonArray allSessions;
    QJsonArray active = m_sessionModel->getAllSessions();
    for (int i = 0; i < active.size(); ++i) {
      allSessions.append(active[i]);
    }
    QJsonArray archived = m_archiveModel->getAllSessions();
    for (int i = 0; i < archived.size(); ++i) {
      allSessions.append(archived[i]);
    }
    m_sourceModel->recalculateStatsFromSessions(allSessions);
  };

  connect(m_sessionModel, &SessionModel::sessionsLoadedOrUpdated, this,
          updateSourceStats);
  connect(m_archiveModel, &SessionModel::sessionsLoadedOrUpdated, this,
          updateSourceStats);

  m_sessionModel->loadSessions();
  m_archiveModel->loadSessions();

  for (int i = 0; i < m_archiveModel->rowCount(); ++i) {
    QString prUrl =
        m_archiveModel
            ->data(m_archiveModel->index(i, 0), SessionModel::PrUrlRole)
            .toString();
    if (!prUrl.isEmpty()) {
      m_apiManager->fetchGithubPullRequest(prUrl);
    }
  }

  // Manually trigger calculation once after load
  updateSourceStats();

  // Initial refresh
  QTimer::singleShot(0, this, [this]() { refreshSources(); });
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event) {
  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  bool closeToTray = config.readEntry("CloseToTray", false);
  if (closeToTray) {
    hide();
    event->ignore();
  } else {
    KXmlGuiWindow::closeEvent(event);
  }
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);

  // Sources View
  QWidget *srcTab = new QWidget(this);
  QVBoxLayout *srcLayout = new QVBoxLayout(srcTab);
  m_sourcesFilterEditor = new FilterEditor(this);
  m_sourcesFilterEditor->setSimplifiedMode(true);
  srcLayout->addWidget(m_sourcesFilterEditor);
  m_sourceView = new QTreeView(this);
  srcLayout->addWidget(m_sourceView);

  AdvancedFilterProxyModel *proxyModel = new AdvancedFilterProxyModel(this);
  proxyModel->setSourceModel(m_sourceModel);

  m_sourceView->setModel(proxyModel);
  m_sourceView->setSortingEnabled(true);
  m_sourceView->header()->setSectionResizeMode(SourceModel::ColName,
                                               QHeaderView::Stretch);
  m_sourceView->header()->setMinimumSectionSize(300);
  m_sourceView->header()->resizeSection(SourceModel::ColName, 400);
  m_sourceView->sortByColumn(SourceModel::ColLastUsed, Qt::DescendingOrder);
  m_sourceView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sourceView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_sourceView->header()->setStretchLastSection(false);

  m_sourceView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sourceView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sourceView->indexAt(pos);
        if (index.isValid()) {
          if (!m_sourceView->selectionModel()->isSelected(index)) {
            m_sourceView->setCurrentIndex(index);
          }
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(
                  m_sourceView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
          QString id =
              m_sourceModel->data(sourceIndex, SourceModel::IdRole).toString();
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
          QAction *newSessionActionLocal = menu.addAction(i18n("New Session"));
          connect(newSessionActionLocal, &QAction::triggered,
                  [this, sourceIndex]() {
                    QModelIndexList selection =
                        m_sourceView->selectionModel()->selectedIndexes();
                    if (selection.isEmpty()) {
                      onSourceActivated(sourceIndex);
                    } else {
                      onSourceActivated(selection.first());
                    }
                  });

          QAction *sourceViewSessionsAction =
              menu.addAction(i18n("View Sessions"));
          connect(sourceViewSessionsAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sourceView->selectionModel()->selectedRows();
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sourceView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              QString currentId =
                  m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                      .toString();
              SessionsWindow *window = new SessionsWindow(
                  currentId, m_apiManager, m_sessionModel, this);
              connect(window, &SessionsWindow::watchRequested, this,
                      [this](const QJsonObject &s) {
                        m_sessionModel->addSession(s);
                        m_sessionModel->saveSessions();
                      });
              window->show();
            }
          });
          menu.addAction(m_refreshSourceAction);
          menu.addAction(m_viewSessionsAction);
          menu.addAction(m_showPastNewSessionsAction);
          menu.addAction(m_viewRawDataAction);
          menu.addAction(m_openUrlAction);
          menu.addAction(m_copyUrlAction);

          QJsonObject rawData =
              m_sourceModel->data(sourceIndex, SourceModel::RawDataRole)
                  .toJsonObject();
          if (rawData.contains(QStringLiteral("github"))) {
            QJsonObject github =
                rawData.value(QStringLiteral("github")).toObject();
            QMenu *githubMenu = menu.addMenu(i18n("GitHub"));

            auto addGithubLink = [this, githubMenu,
                                  urlStr](const QString &title,
                                          const QString &path) {
              QAction *openAction =
                  githubMenu->addAction(i18n("Open %1", title));
              connect(openAction, &QAction::triggered, [urlStr, path]() {
                QDesktopServices::openUrl(QUrl(urlStr + path));
              });
              QAction *copyAction =
                  githubMenu->addAction(i18n("Copy %1 URL", title));
              connect(copyAction, &QAction::triggered, [this, urlStr, path]() {
                QGuiApplication::clipboard()->setText(urlStr + path);
                updateStatus(i18n("URL copied to clipboard."));
              });
            };

            if (github.value(QStringLiteral("has_wiki")).toBool()) {
              addGithubLink(QStringLiteral("Wiki"), QStringLiteral("/wiki"));
            }
            if (github.value(QStringLiteral("has_discussions")).toBool()) {
              addGithubLink(QStringLiteral("Discussions"),
                            QStringLiteral("/discussions"));
            }
            if (github.value(QStringLiteral("has_issues")).toBool()) {
              addGithubLink(QStringLiteral("Issues"),
                            QStringLiteral("/issues"));
            }

            QString homepage =
                github.value(QStringLiteral("homepage")).toString();
            if (!homepage.isEmpty()) {
              QAction *openAction = githubMenu->addAction(i18n("Open Website"));
              connect(openAction, &QAction::triggered, [homepage]() {
                QDesktopServices::openUrl(QUrl(homepage));
              });
              QAction *copyAction =
                  githubMenu->addAction(i18n("Copy Website URL"));
              connect(copyAction, &QAction::triggered, [this, homepage]() {
                QGuiApplication::clipboard()->setText(homepage);
                updateStatus(i18n("URL copied to clipboard."));
              });
            }
          }

          menu.exec(m_sourceView->mapToGlobal(pos));
        }
      });
  connect(m_sourceView, &QTreeView::doubleClicked, this,
          &MainWindow::onSourceActivated);
  m_tabWidget->addTab(srcTab, i18n("Sources"));

  // Sessions View
  QWidget *pastTab = new QWidget(this);
  QVBoxLayout *pastLayout = new QVBoxLayout(pastTab);
  m_pastFilterEditor = new FilterEditor(this);
  pastLayout->addWidget(m_pastFilterEditor);

  m_sessionView = new QTreeView(this);
  pastLayout->addWidget(m_sessionView);

  AdvancedFilterProxyModel *sessionProxyModel =
      new AdvancedFilterProxyModel(this);
  sessionProxyModel->setSourceModel(m_sessionModel);
  m_sessionView->setModel(sessionProxyModel);
  m_sessionView->setSortingEnabled(true);
  m_sessionView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sessionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_sessionView->header()->setStretchLastSection(true);
  m_sessionView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sessionView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          if (!m_sessionView->selectionModel()->isSelected(index)) {
            m_sessionView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_sessionView->setCurrentIndex(index);
          }
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(
                  m_sessionView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

          QMenu menu;
          QAction *openSessionAction = menu.addAction(i18n("Open Session"));
          QAction *openSessionsForSourceAction =
              menu.addAction(i18n("Open Sessions for source"));
          QAction *refreshSessionAction =
              menu.addAction(i18n("Refresh session details"));
          menu.addSeparator();

          QString id = m_sessionModel->data(sourceIndex, SessionModel::IdRole)
                           .toString();
          QAction *openJulesUrlAction = nullptr;
          QAction *copyJulesUrlAction = nullptr;
          if (!id.isEmpty()) {
            openJulesUrlAction = menu.addAction(i18n("Open Jules URL"));
            copyJulesUrlAction = menu.addAction(i18n("Copy Jules URL"));
          }

          QString prUrl =
              m_sessionModel->data(sourceIndex, SessionModel::PrUrlRole)
                  .toString();
          QAction *openGithubUrlAction = nullptr;
          QAction *copyGithubUrlAction = nullptr;
          if (!prUrl.isEmpty()) {
            openGithubUrlAction = menu.addAction(i18n("Open Github URL"));
            copyGithubUrlAction = menu.addAction(i18n("Copy Github URL"));
          }

          menu.addSeparator();
          QAction *archiveAction = menu.addAction(i18n("Archive"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));
          menu.addSeparator();
          QAction *newSessionFromSessionAction =
              menu.addAction(i18n("New Session From Session"));
          QAction *copyTemplateAction =
              menu.addAction(i18n("Copy as Template"));

          connect(openSessionAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sessionView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              onSessionActivated(idx);
            }
          });

          connect(openSessionsForSourceAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sessionView->selectionModel()->selectedRows();
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sessionView->model());
            QStringList processedSources;
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              QString source =
                  m_sessionModel->data(mappedIdx, SessionModel::SourceRole)
                      .toString();
              if (!processedSources.contains(source) && !source.isEmpty()) {
                processedSources.append(source);
                SessionsWindow *window = new SessionsWindow(
                    source, m_apiManager, m_sessionModel, this);
                window->show();
              }
            }
          });

          connect(refreshSessionAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sessionView->selectionModel()->selectedRows();
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sessionView->model());
            int count = 0;
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              QString currentId =
                  m_sessionModel->data(mappedIdx, SessionModel::IdRole)
                      .toString();
              if (!currentId.isEmpty()) {
                m_apiManager->reloadSession(currentId);
                count++;
              }
            }
            updateStatus(i18np("Refreshing 1 session...",
                               "Refreshing %1 sessions...", count));
          });

          if (openJulesUrlAction && copyJulesUrlAction) {
            connect(openJulesUrlAction, &QAction::triggered, [this]() {
              QModelIndexList selectedRows =
                  m_sessionView->selectionModel()->selectedRows();
              const QSortFilterProxyModel *proxy =
                  qobject_cast<const QSortFilterProxyModel *>(
                      m_sessionView->model());
              int count = 0;
              for (const QModelIndex &idx : selectedRows) {
                QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
                QString currentId =
                    m_sessionModel->data(mappedIdx, SessionModel::IdRole)
                        .toString();
                if (!currentId.isEmpty()) {
                  QString urlStr =
                      QStringLiteral("https://jules.google.com/sessions/") +
                      currentId;
                  QDesktopServices::openUrl(QUrl(urlStr));
                  count++;
                }
              }
              updateStatus(
                  i18np("Opened 1 session", "Opened %1 sessions", count));
            });
            connect(copyJulesUrlAction, &QAction::triggered, [this]() {
              QModelIndexList selectedRows =
                  m_sessionView->selectionModel()->selectedRows();
              const QSortFilterProxyModel *proxy =
                  qobject_cast<const QSortFilterProxyModel *>(
                      m_sessionView->model());
              QStringList urls;
              for (const QModelIndex &idx : selectedRows) {
                QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
                QString currentId =
                    m_sessionModel->data(mappedIdx, SessionModel::IdRole)
                        .toString();
                if (!currentId.isEmpty()) {
                  urls.append(
                      QStringLiteral("https://jules.google.com/sessions/") +
                      currentId);
                }
              }
              if (!urls.isEmpty()) {
                QGuiApplication::clipboard()->setText(
                    urls.join(QLatin1Char('\n')));
                updateStatus(i18np("1 Jules URL copied to clipboard.",
                                   "%1 Jules URLs copied to clipboard.",
                                   urls.size()));
              }
            });
          }

          if (openGithubUrlAction && copyGithubUrlAction) {
            connect(openGithubUrlAction, &QAction::triggered, [this]() {
              QModelIndexList selectedRows =
                  m_sessionView->selectionModel()->selectedRows();
              const QSortFilterProxyModel *proxy =
                  qobject_cast<const QSortFilterProxyModel *>(
                      m_sessionView->model());
              int count = 0;
              for (const QModelIndex &idx : selectedRows) {
                QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
                QString currentPrUrl =
                    m_sessionModel->data(mappedIdx, SessionModel::PrUrlRole)
                        .toString();
                if (!currentPrUrl.isEmpty()) {
                  QDesktopServices::openUrl(QUrl(currentPrUrl));
                  count++;
                }
              }
              updateStatus(
                  i18np("Opened 1 Github URL", "Opened %1 Github URLs", count));
            });
            connect(copyGithubUrlAction, &QAction::triggered, [this]() {
              QModelIndexList selectedRows =
                  m_sessionView->selectionModel()->selectedRows();
              const QSortFilterProxyModel *proxy =
                  qobject_cast<const QSortFilterProxyModel *>(
                      m_sessionView->model());
              QStringList urls;
              for (const QModelIndex &idx : selectedRows) {
                QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
                QString currentPrUrl =
                    m_sessionModel->data(mappedIdx, SessionModel::PrUrlRole)
                        .toString();
                if (!currentPrUrl.isEmpty()) {
                  urls.append(currentPrUrl);
                }
              }
              if (!urls.isEmpty()) {
                QGuiApplication::clipboard()->setText(
                    urls.join(QLatin1Char('\n')));
                updateStatus(i18np("1 Github URL copied to clipboard.",
                                   "%1 Github URLs copied to clipboard.",
                                   urls.size()));
              }
            });
          }

          connect(archiveAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sessionView->selectionModel()->selectedRows();
            QList<int> rowsToArchive;
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sessionView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              if (!rowsToArchive.contains(mappedIdx.row())) {
                rowsToArchive.append(mappedIdx.row());
              }
            }
            std::sort(rowsToArchive.begin(), rowsToArchive.end(),
                      std::greater<int>());

            for (int row : rowsToArchive) {
              QJsonObject session = m_sessionModel->getSession(row);
              m_archiveModel->addSession(session);
              m_sessionModel->removeSession(row);
            }
            m_archiveModel->saveSessions();
            updateStatus(i18np("1 session archived.", "%1 sessions archived.",
                               rowsToArchive.size()));
          });

          connect(deleteAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_sessionView->selectionModel()->selectedRows();
            QList<int> rowsToDelete;
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sessionView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              if (!rowsToDelete.contains(mappedIdx.row())) {
                rowsToDelete.append(mappedIdx.row());
              }
            }
            std::sort(rowsToDelete.begin(), rowsToDelete.end(),
                      std::greater<int>());

            for (int row : rowsToDelete) {
              m_sessionModel->removeSession(row);
            }
            updateStatus(i18np("1 session deleted.", "%1 sessions deleted.",
                               rowsToDelete.size()));
          });

          connect(newSessionFromSessionAction, &QAction::triggered,
                  [this, sourceIndex]() {
                    QJsonObject session =
                        m_sessionModel->getSession(sourceIndex.row());
                    QString prompt =
                        session.value(QStringLiteral("prompt")).toString();
                    QString source =
                        session.value(QStringLiteral("sourceContext"))
                            .toObject()
                            .value(QStringLiteral("source"))
                            .toString();
                    QJsonObject initData;
                    initData[QStringLiteral("prompt")] = prompt;
                    if (!source.isEmpty()) {
                      QJsonArray sourcesArr;
                      sourcesArr.append(source);
                      initData[QStringLiteral("sources")] = sourcesArr;
                    }
                    bool hasApiKey = !m_apiManager->apiKey().isEmpty();
                    NewSessionDialog dialog(m_sourceModel, m_templatesModel,
                                            hasApiKey, this);
                    dialog.setInitialData(initData);
                    connect(&dialog, &NewSessionDialog::createSessionRequested,
                            this, &MainWindow::onSessionCreated);
                    connect(&dialog, &NewSessionDialog::saveDraftRequested,
                            this, &MainWindow::onDraftSaved);
                    dialog.exec();
                  });

          connect(copyTemplateAction, &QAction::triggered, [this, index]() {
            SaveDialog dlg(QStringLiteral("Template"), this);
            if (dlg.exec() == QDialog::Accepted) {
              QJsonObject sessionData;
              QString prompt =
                  m_sessionModel->data(index, SessionModel::PromptRole)
                      .toString();
              QString source =
                  m_sessionModel->data(index, SessionModel::SourceRole)
                      .toString();
              sessionData[QStringLiteral("prompt")] = prompt;
              sessionData[QStringLiteral("source")] = source;
              sessionData[QStringLiteral("name")] = dlg.nameOrComment();
              sessionData[QStringLiteral("description")] = dlg.description();
              m_templatesModel->addTemplate(sessionData);
              updateStatus(i18n("Template created from session."));
            }
          });

          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QTreeView::doubleClicked, this,
          &MainWindow::onSessionActivated);

  m_tabWidget->addTab(pastTab, i18n("Past"));
  // Archive View
  QWidget *archTab = new QWidget(this);
  QVBoxLayout *archLayout = new QVBoxLayout(archTab);
  m_archiveFilterEditor = new FilterEditor(this);
  archLayout->addWidget(m_archiveFilterEditor);
  m_archiveView = new QTreeView(this);
  archLayout->addWidget(m_archiveView);
  AdvancedFilterProxyModel *archiveProxyModel =
      new AdvancedFilterProxyModel(this);
  archiveProxyModel->setSourceModel(m_archiveModel);
  m_archiveView->setModel(archiveProxyModel);
  m_archiveView->setSortingEnabled(true);
  m_archiveView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_archiveView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_archiveView->header()->setStretchLastSection(true);

  m_archiveView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_archiveView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_archiveView->indexAt(pos);
        if (index.isValid()) {
          if (!m_archiveView->selectionModel()->isSelected(index)) {
            m_archiveView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_archiveView->setCurrentIndex(index);
          }
          QMenu menu;
          QAction *openSessionAction = menu.addAction(i18n("Open Session"));
          QAction *unarchiveAction = menu.addAction(i18n("Unarchive"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));
          menu.addSeparator();
          QAction *copyTemplateAction =
              menu.addAction(i18n("Copy as Template"));

          connect(openSessionAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_archiveView->selectionModel()->selectedRows();
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_archiveView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              QJsonObject sessionData =
                  m_archiveModel->getSession(mappedIdx.row());
              if (!sessionData.isEmpty()) {
                SessionWindow *window =
                    new SessionWindow(sessionData, m_apiManager, this);
                connectSessionWindow(window);
                window->show();
              } else {
                QString id =
                    m_archiveModel->data(mappedIdx, SessionModel::IdRole)
                        .toString();
                m_apiManager->getSession(id);
                updateStatus(i18n("Fetching details for session %1...", id));
              }
            }
          });

          connect(unarchiveAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_archiveView->selectionModel()->selectedRows();
            QList<int> rowsToUnarchive;
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_archiveView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              if (!rowsToUnarchive.contains(mappedIdx.row())) {
                rowsToUnarchive.append(mappedIdx.row());
              }
            }
            std::sort(rowsToUnarchive.begin(), rowsToUnarchive.end(),
                      std::greater<int>());

            for (int row : rowsToUnarchive) {
              QJsonObject session = m_archiveModel->getSession(row);
              m_sessionModel->addSession(session);
              m_archiveModel->removeSession(row);
            }
            m_sessionModel->saveSessions();
            updateStatus(i18np("1 session unarchived.",
                               "%1 sessions unarchived.",
                               rowsToUnarchive.size()));
          });

          connect(deleteAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_archiveView->selectionModel()->selectedRows();
            QList<int> rowsToDelete;
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_archiveView->model());
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              if (!rowsToDelete.contains(mappedIdx.row())) {
                rowsToDelete.append(mappedIdx.row());
              }
            }
            std::sort(rowsToDelete.begin(), rowsToDelete.end(),
                      std::greater<int>());

            for (int row : rowsToDelete) {
              m_archiveModel->removeSession(row);
            }
            updateStatus(i18np("1 session deleted from archive.",
                               "%1 sessions deleted from archive.",
                               rowsToDelete.size()));
          });

          connect(copyTemplateAction, &QAction::triggered, [this, index]() {
            SaveDialog dlg(QStringLiteral("Template"), this);
            if (dlg.exec() == QDialog::Accepted) {
              QJsonObject sessionData;
              QString prompt =
                  m_archiveModel->data(index, SessionModel::PromptRole)
                      .toString();
              QString source =
                  m_archiveModel->data(index, SessionModel::SourceRole)
                      .toString();
              sessionData[QStringLiteral("prompt")] = prompt;
              sessionData[QStringLiteral("source")] = source;
              sessionData[QStringLiteral("name")] = dlg.nameOrComment();
              sessionData[QStringLiteral("description")] = dlg.description();
              m_templatesModel->addTemplate(sessionData);
              updateStatus(i18n("Template created from archived session."));
            }
          });

          menu.exec(m_archiveView->mapToGlobal(pos));
        }
      });
  connect(
      m_archiveView, &QTreeView::doubleClicked, this,
      [this](const QModelIndex &index) {
        const QSortFilterProxyModel *proxy =
            qobject_cast<const QSortFilterProxyModel *>(m_archiveView->model());
        QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
        QJsonObject sessionData = m_archiveModel->getSession(sourceIndex.row());
        if (!sessionData.isEmpty()) {
          SessionWindow *window =
              new SessionWindow(sessionData, m_apiManager, this);
          connectSessionWindow(window);
          window->show();
        } else {
          QString id = m_archiveModel->data(sourceIndex, SessionModel::IdRole)
                           .toString();
          m_apiManager->getSession(id);
          updateStatus(i18n("Fetching details for session %1...", id));
        }
      });

  m_tabWidget->addTab(archTab, i18n("Archive"));

  // Drafts View
  QWidget *draftsTab = new QWidget(this);
  QVBoxLayout *draftsLayout = new QVBoxLayout(draftsTab);
  m_draftsFilter = new QLineEdit(this);
  m_draftsFilter->setPlaceholderText(i18n("Filter drafts..."));
  draftsLayout->addWidget(m_draftsFilter);
  m_draftsView = new QListView(this);
  draftsLayout->addWidget(m_draftsView);
  QSortFilterProxyModel *draftsProxy = new QSortFilterProxyModel(this);
  draftsProxy->setSourceModel(m_draftsModel);
  draftsProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_draftsView->setModel(draftsProxy);
  m_draftsView->setItemDelegate(new DraftDelegate(this));
  m_draftsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_draftsView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_draftsView->indexAt(pos);
        if (index.isValid()) {
          if (!m_draftsView->selectionModel()->isSelected(index)) {
            m_draftsView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_draftsView->setCurrentIndex(index);
          }
          QMenu menu;
          QAction *submitAction = menu.addAction(i18n("Submit Now"));
          QAction *duplicateAction = menu.addAction(i18n("Duplicate"));
          QAction *copyTemplateAction =
              menu.addAction(i18n("Copy as Template"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));

          connect(submitAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_draftsView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              onDraftActivated(idx);
            }
          });

          connect(duplicateAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_draftsView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              QJsonObject draft = m_draftsModel->getDraft(idx.row());
              m_draftsModel->addDraft(draft);
            }
            updateStatus(i18np("1 draft duplicated.", "%1 drafts duplicated.",
                               selectedRows.size()));
          });

          connect(copyTemplateAction, &QAction::triggered, [this, index]() {
            SaveDialog dlg(QStringLiteral("Template"), this);
            if (dlg.exec() == QDialog::Accepted) {
              QJsonObject draft = m_draftsModel->getDraft(index.row());
              draft[QStringLiteral("name")] = dlg.nameOrComment();
              draft[QStringLiteral("description")] = dlg.description();
              m_templatesModel->addTemplate(draft);
              updateStatus(i18n("Template created from draft."));
            }
          });

          connect(deleteAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_draftsView->selectionModel()->selectedRows();
            if (QMessageBox::question(
                    this,
                    i18np("Delete Draft", "Delete Drafts", selectedRows.size()),
                    i18np("Are you sure?",
                          "Are you sure you want to delete these drafts?",
                          selectedRows.size())) == QMessageBox::Yes) {
              QList<int> rowsToDelete;
              for (const QModelIndex &idx : selectedRows) {
                if (!rowsToDelete.contains(idx.row())) {
                  rowsToDelete.append(idx.row());
                }
              }
              std::sort(rowsToDelete.begin(), rowsToDelete.end(),
                        std::greater<int>());

              for (int row : rowsToDelete) {
                m_draftsModel->removeDraft(row);
              }
            }
          });

          menu.exec(m_draftsView->mapToGlobal(pos));
        }
      });
  connect(m_draftsView, &QListView::doubleClicked, this,
          &MainWindow::onDraftActivated);

  m_tabWidget->addTab(draftsTab, i18n("Drafts"));

  // Templates View
  QWidget *tplTab = new QWidget(this);
  QVBoxLayout *tplLayout = new QVBoxLayout(tplTab);
  m_templatesFilter = new QLineEdit(this);
  m_templatesFilter->setPlaceholderText(i18n("Filter templates..."));
  tplLayout->addWidget(m_templatesFilter);
  m_templatesView = new QListView(this);
  tplLayout->addWidget(m_templatesView);
  QSortFilterProxyModel *tplProxy = new QSortFilterProxyModel(this);
  tplProxy->setSourceModel(m_templatesModel);
  tplProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_templatesView->setModel(tplProxy);
  m_templatesView->setItemDelegate(new DraftDelegate(this));
  m_templatesView->setContextMenuPolicy(Qt::CustomContextMenu);
  m_templatesView->setDragEnabled(true);
  m_templatesView->setAcceptDrops(true);
  m_templatesView->setDropIndicatorShown(true);
  m_templatesView->setDragDropMode(QAbstractItemView::DragDrop);
  connect(
      m_templatesView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_templatesView->indexAt(pos);
        QMenu menu;
        if (index.isValid()) {
          if (!m_templatesView->selectionModel()->isSelected(index)) {
            m_templatesView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_templatesView->setCurrentIndex(index);
          }
          QAction *useAction = menu.addAction(i18n("Use Template"));
          connect(useAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_templatesView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              onTemplateActivated(idx);
            }
          });

          QAction *editAction = menu.addAction(i18n("Edit Template"));
          connect(editAction, &QAction::triggered, [this, index]() {
            TemplateEditDialog dlg(this);
            dlg.setInitialData(m_templatesModel->getTemplate(index.row()));
            if (dlg.exec() == QDialog::Accepted) {
              m_templatesModel->updateTemplate(index.row(), dlg.templateData());
              updateStatus(i18n("Template updated."));
            }
          });

          QAction *copyClipboardAction =
              menu.addAction(i18n("Copy to Clipboard"));
          connect(copyClipboardAction, &QAction::triggered,
                  [this, index]() { copyTemplateToClipboard(index); });
        }

        QAction *pasteClipboardActionOuter =
            menu.addAction(i18n("Paste from Clipboard"));
        connect(pasteClipboardActionOuter, &QAction::triggered,
                [this]() { pasteTemplateFromClipboard(); });

        if (index.isValid()) {
          QAction *exportSingleAction =
              menu.addAction(i18n("Export Template..."));
          connect(exportSingleAction, &QAction::triggered, [this, index]() {
            QString filePath = QFileDialog::getSaveFileName(
                this, i18n("Export Template"), QString(),
                i18n("JSON Files (*.json)"));
            if (filePath.isEmpty())
              return;

            QJsonArray exportArray;
            exportArray.append(m_templatesModel->getTemplate(index.row()));
            QJsonDocument doc(exportArray);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
              file.write(doc.toJson(QJsonDocument::Indented));
              file.close();
              updateStatus(i18n("Template exported to %1", filePath));
            } else {
              updateStatus(i18n("Failed to export template to %1", filePath));
            }
          });

          QAction *deleteAction = menu.addAction(i18n("Delete Template"));
          connect(deleteAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_templatesView->selectionModel()->selectedRows();
            if (QMessageBox::question(
                    this,
                    i18np("Delete Template", "Delete Templates",
                          selectedRows.size()),
                    i18np("Are you sure you want to delete this template?",
                          "Are you sure you want to delete these templates?",
                          selectedRows.size())) == QMessageBox::Yes) {
              QList<int> rowsToDelete;
              for (const QModelIndex &idx : selectedRows) {
                if (!rowsToDelete.contains(idx.row())) {
                  rowsToDelete.append(idx.row());
                }
              }
              std::sort(rowsToDelete.begin(), rowsToDelete.end(),
                        std::greater<int>());

              for (int row : rowsToDelete) {
                m_templatesModel->removeTemplate(row);
              }
              updateStatus(i18np("1 template deleted.", "%1 templates deleted.",
                                 selectedRows.size()));
            }
          });
        }
        menu.exec(m_templatesView->mapToGlobal(pos));
      });
  connect(m_templatesView, &QListView::doubleClicked, this,
          &MainWindow::onTemplateActivated);

  m_tabWidget->addTab(tplTab, i18n("Templates"));

  // Queue View
  m_queueView = new QListView(this);
  m_queueView->setModel(m_queueModel);
  m_queueView->setItemDelegate(new QueueDelegate(this));
  m_queueView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_queueView, &QListView::activated, this,
          &MainWindow::onQueueActivated);
  connect(m_queueView, &QListView::customContextMenuRequested, this,
          &MainWindow::onQueueContextMenu);
  m_tabWidget->addTab(m_queueView, i18n("Queue"));

  // Errors View
  QWidget *errTab = new QWidget(this);
  QVBoxLayout *errLayout = new QVBoxLayout(errTab);
  m_errorsFilter = new QLineEdit(this);
  m_errorsFilter->setPlaceholderText(i18n("Filter errors..."));
  errLayout->addWidget(m_errorsFilter);
  m_errorsView = new QListView(this);
  errLayout->addWidget(m_errorsView);
  QSortFilterProxyModel *errProxy = new QSortFilterProxyModel(this);
  errProxy->setSourceModel(m_errorsModel);
  errProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_errorsView->setModel(errProxy);
  m_errorsView->setItemDelegate(new DraftDelegate(
      this)); // Reusing DraftDelegate for simple display or create custom
  m_errorsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_errorsView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_errorsView->indexAt(pos);
        if (index.isValid()) {
          if (!m_errorsView->selectionModel()->isSelected(index)) {
            m_errorsView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_errorsView->setCurrentIndex(index);
          }
          QMenu menu;
          QAction *editAction = menu.addAction(i18n("Edit / Modify"));
          QAction *rawTranscriptAction = menu.addAction(i18n("Raw Transcript"));
          QAction *copyTemplateAction =
              menu.addAction(i18n("Copy as Template"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));

          connect(editAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_errorsView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              onErrorActivated(idx);
            }
          });

          connect(copyTemplateAction, &QAction::triggered, [this, index]() {
            SaveDialog dlg(QStringLiteral("Template"), this);
            if (dlg.exec() == QDialog::Accepted) {
              QJsonObject errData = m_errorsModel->getError(index.row());
              QJsonObject req =
                  errData.value(QStringLiteral("request")).toObject();
              req[QStringLiteral("name")] = dlg.nameOrComment();
              req[QStringLiteral("description")] = dlg.description();
              m_templatesModel->addTemplate(req);
              updateStatus(i18n("Template created from error item."));
            }
          });

          connect(rawTranscriptAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_errorsView->selectionModel()->selectedRows();
            for (const QModelIndex &idx : selectedRows) {
              QJsonObject errorData = m_errorsModel->getError(idx.row());
              QJsonObject request =
                  errorData.value(QStringLiteral("request")).toObject();
              QJsonObject response =
                  errorData.value(QStringLiteral("response")).toObject();
              QString errorStr =
                  errorData.value(QStringLiteral("message")).toString();
              QString httpDetails =
                  errorData.value(QStringLiteral("httpDetails")).toString();

              ErrorWindow *window = new ErrorWindow(
                  idx.row(), request,
                  QString::fromUtf8(
                      QJsonDocument(response).toJson(QJsonDocument::Indented)),
                  errorStr, httpDetails, this);
              connect(window, &ErrorWindow::editRequested, [this](int row) {
                QModelIndex idx = m_errorsModel->index(row, 0);
                onErrorActivated(idx);
              });
              connect(window, &ErrorWindow::deleteRequested, [this](int row) {
                m_errorsModel->removeError(row);
                updateStatus(i18n("Error removed."));
              });
              connect(window, &ErrorWindow::draftRequested, [this](int row) {
                QJsonObject errData = m_errorsModel->getError(row);
                QJsonObject req =
                    errData.value(QStringLiteral("request")).toObject();
                m_draftsModel->addDraft(req);
                m_errorsModel->removeError(row);
                updateStatus(i18n("Error converted to draft."));
              });
              connect(window, &ErrorWindow::templateRequested, [this](int row) {
                SaveDialog dlg(QStringLiteral("Template"), this);
                if (dlg.exec() == QDialog::Accepted) {
                  QJsonObject errData = m_errorsModel->getError(row);
                  QJsonObject req =
                      errData.value(QStringLiteral("request")).toObject();
                  req[QStringLiteral("name")] = dlg.nameOrComment();
                  req[QStringLiteral("description")] = dlg.description();
                  m_templatesModel->addTemplate(req);
                  updateStatus(i18n("Template created from error item."));
                }
              });
              connect(window, &ErrorWindow::sendNowRequested, [this](int row) {
                QJsonObject errData = m_errorsModel->getError(row);
                QJsonObject req =
                    errData.value(QStringLiteral("request")).toObject();
                m_errorsModel->removeError(row);
                m_apiManager->createSessionAsync(req);
                updateStatus(i18n("Sending error item immediately..."));
              });

              window->setAttribute(Qt::WA_DeleteOnClose);
              window->show();
            }
          });

          connect(deleteAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_errorsView->selectionModel()->selectedRows();
            if (QMessageBox::question(
                    this,
                    i18np("Delete Error", "Delete Errors", selectedRows.size()),
                    i18np("Are you sure?",
                          "Are you sure you want to delete these errors?",
                          selectedRows.size())) == QMessageBox::Yes) {
              QList<int> rowsToDelete;
              for (const QModelIndex &idx : selectedRows) {
                if (!rowsToDelete.contains(idx.row())) {
                  rowsToDelete.append(idx.row());
                }
              }
              std::sort(rowsToDelete.begin(), rowsToDelete.end(),
                        std::greater<int>());

              for (int row : rowsToDelete) {
                m_errorsModel->removeError(row);
              }
            }
          });

          menu.exec(m_errorsView->mapToGlobal(pos));
        }
      });
  connect(m_errorsView, &QListView::doubleClicked, this,
          &MainWindow::onErrorActivated);

  m_tabWidget->addTab(errTab, i18n("Errors"));

  mainLayout->addWidget(m_tabWidget);

  // Toolbar is handled by KXmlGuiWindow via kjulesui.rc

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

  // Connect model signals to update tab titles with counts
  connect(m_draftsModel, &QAbstractListModel::rowsInserted, this,
          &MainWindow::updateTabTitles);
  connect(m_draftsModel, &QAbstractListModel::rowsRemoved, this,
          &MainWindow::updateTabTitles);
  connect(m_draftsModel, &QAbstractListModel::modelReset, this,
          &MainWindow::updateTabTitles);

  connect(m_templatesModel, &QAbstractListModel::rowsInserted, this,
          &MainWindow::updateTabTitles);
  connect(m_templatesModel, &QAbstractListModel::rowsRemoved, this,
          &MainWindow::updateTabTitles);
  connect(m_templatesModel, &QAbstractListModel::modelReset, this,
          &MainWindow::updateTabTitles);

  connect(m_errorsModel, &QAbstractListModel::rowsInserted, this,
          &MainWindow::updateTabTitles);
  connect(m_errorsModel, &QAbstractListModel::rowsRemoved, this,
          &MainWindow::updateTabTitles);
  connect(m_errorsModel, &QAbstractListModel::modelReset, this,
          &MainWindow::updateTabTitles);

  // Initial title update
  updateTabTitles();
}

void MainWindow::updateTabTitles() {
  if (!m_tabWidget)
    return;

  for (int i = 0; i < m_tabWidget->count(); ++i) {
    QWidget *page = m_tabWidget->widget(i);
    if (page == m_draftsView) {
      int count = m_draftsModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Drafts (%1)", count)
                                           : i18n("Drafts"));
    } else if (page == m_templatesView) {
      int count = m_templatesModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Templates (%1)", count)
                                           : i18n("Templates"));
    } else if (page == m_errorsView) {
      int count = m_errorsModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Errors (%1)", count)
                                           : i18n("Errors"));
    }
  }
}

void MainWindow::onGithubPullRequestInfoReceived(const QString &prUrl,
                                                 const QJsonObject &info) {
  for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
    QModelIndex index = m_sessionModel->index(i, 0);
    if (m_sessionModel->data(index, SessionModel::PrUrlRole).toString() ==
        prUrl) {
      QJsonObject session = m_sessionModel->getSession(i);
      session[QStringLiteral("githubPrInfo")] = info;
      m_sessionModel->updateSession(session);
      // Let's also update archive model and watch model if needed.
      // Actually, since they all use SessionModel, we'll iterate through them.
    }
  }

  for (int i = 0; i < m_archiveModel->rowCount(); ++i) {
    QModelIndex index = m_archiveModel->index(i, 0);
    if (m_archiveModel->data(index, SessionModel::PrUrlRole).toString() ==
        prUrl) {
      QJsonObject session = m_archiveModel->getSession(i);
      session[QStringLiteral("githubPrInfo")] = info;
      m_archiveModel->updateSession(session);
    }
  }
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon(QStringLiteral(":/icons/kjules-tray.png")));
  m_trayIcon->setToolTip(i18n("Google Jules Client"));

  m_trayMenu = new QMenu(this);

  QAction *showHideAction = new QAction(i18n("Show/Hide"), this);
  connect(showHideAction, &QAction::triggered, this,
          &MainWindow::toggleWindowVisibility);
  m_trayMenu->addAction(showHideAction);

  m_trayMenu->addSeparator();

  QAction *newSessionAction = new QAction(i18n("New Session"), this);
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);
  m_trayMenu->addAction(newSessionAction);

  m_trayMenu->addSeparator();

  m_trayMenu->addAction(m_viewSessionsAction);

  m_trayMenu->addSeparator();

  QAction *quitAction = new QAction(i18n("&Quit"), this);
  connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
  m_trayMenu->addAction(quitAction);

  m_trayIcon->setContextMenu(m_trayMenu);
  m_trayIcon->show();

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &MainWindow::onTrayIconActivated);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger ||
      reason == QSystemTrayIcon::DoubleClick) {
    toggleWindowVisibility();
  } else if (reason == QSystemTrayIcon::Context) {
    m_trayMenu->popup(QCursor::pos());
  }
}

void MainWindow::createActions() {
  QAction *newSessionAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-new")),
                  i18n("New Session"), this);
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);
  actionCollection()->addAction(QStringLiteral("new_session"),
                                newSessionAction);
  KGlobalAccel::setGlobalShortcut(newSessionAction, QKeySequence());
  actionCollection()->setDefaultShortcut(newSessionAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_N));

  m_showFullSessionListAction =
      new QAction(i18n("Show Full Session List"), this);
  actionCollection()->addAction(QStringLiteral("show_full_session_list"),
                                m_showFullSessionListAction);
  connect(m_showFullSessionListAction, &QAction::triggered, this, [this]() {
    SessionsWindow *window =
        new SessionsWindow(QString(), m_apiManager, m_sessionModel, this);
    connect(window, &SessionsWindow::watchRequested, this,
            [this](const QJsonObject &s) {
              m_sessionModel->addSession(s);
              m_sessionModel->saveSessions();
            });

    window->show();
  });

  m_refreshSourcesAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sources"), this);
  actionCollection()->setDefaultShortcut(m_refreshSourcesAction,
                                         QKeySequence(Qt::Key_F5));
  connect(m_refreshSourcesAction, &QAction::triggered, this,
          &MainWindow::refreshSources);
  actionCollection()->addAction(QStringLiteral("refresh_sources"),
                                m_refreshSourcesAction);

  m_refreshSourceAction = new QAction(i18n("Refresh Source"), this);
  actionCollection()->addAction(QStringLiteral("refresh_source"),
                                m_refreshSourceAction);

  m_recalculateStatsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("tools-wizard")),
                  i18n("Recalculate Session Stats"), this);
  actionCollection()->addAction(QStringLiteral("recalculate_stats"),
                                m_recalculateStatsAction);
  connect(m_recalculateStatsAction, &QAction::triggered, this, [this]() {
    QJsonArray allSessions;
    QJsonArray active = m_sessionModel->getAllSessions();
    for (int i = 0; i < active.size(); ++i) {
      allSessions.append(active[i]);
    }
    QJsonArray archived = m_archiveModel->getAllSessions();
    for (int i = 0; i < archived.size(); ++i) {
      allSessions.append(archived[i]);
    }
    m_sourceModel->recalculateStatsFromSessions(allSessions);
    updateStatus(i18n("Session statistics recalculated successfully."));
  });
  connect(m_refreshSourceAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();
      m_apiManager->getSource(id);
      count++;
    }
    updateStatus(
        i18np("Refreshing 1 source...", "Refreshing %1 sources...", count));
  });

  QAction *toggleWindowAction =
      new QAction(QIcon::fromTheme(QStringLiteral("window-minimize")),
                  i18n("Minimize to Tray"), this);
  connect(toggleWindowAction, &QAction::triggered, this,
          &MainWindow::toggleWindowVisibility);
  actionCollection()->addAction(QStringLiteral("toggle_window"),
                                toggleWindowAction);
  KGlobalAccel::setGlobalShortcut(toggleWindowAction, QKeySequence());
  actionCollection()->setDefaultShortcut(toggleWindowAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_M));

  m_viewSessionsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-list-details")),
                  i18n("View Sessions"), this);
  actionCollection()->addAction(QStringLiteral("view_sessions"),
                                m_viewSessionsAction);
  connect(m_viewSessionsAction, &QAction::triggered, this, [this]() {
    SessionsWindow *window =
        new SessionsWindow(QString(), m_apiManager, m_sessionModel, this);
    connect(window, &SessionsWindow::watchRequested, this,
            [this](const QJsonObject &s) {
              m_sessionModel->addSession(s);
              m_sessionModel->saveSessions();
            });

    window->show();
  });

  m_showPastNewSessionsAction =
      new QAction(i18n("Show past new sessions"), this);
  actionCollection()->addAction(QStringLiteral("show_past_new_sessions"),
                                m_showPastNewSessionsAction);
  connect(m_showPastNewSessionsAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

      QString path =
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QFile file(path + QStringLiteral("/cached_sessions.json"));
      QJsonArray cachedSessions;
      if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        cachedSessions = doc.array();
        file.close();
      }
      QJsonArray filteredSessions;
      for (int i = 0; i < cachedSessions.size(); ++i) {
        QJsonObject session = cachedSessions[i].toObject();
        QString sessionSource = session.value(QStringLiteral("sourceContext"))
                                    .toObject()
                                    .value(QStringLiteral("source"))
                                    .toString();
        if (sessionSource == id) {
          filteredSessions.append(session);
        }
      }
      KXmlGuiWindow *sessionsWindow = new KXmlGuiWindow(this);
      sessionsWindow->setObjectName(
          QStringLiteral("PastNewSessions_%1").arg(id));
      SessionModel *localModel = new SessionModel(
          QStringLiteral("cached_all_sessions.json"), sessionsWindow);
      localModel->setSessions(filteredSessions);
      sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
      sessionsWindow->setWindowTitle(i18n("Past New Sessions for %1", id));
      QListView *listView = new QListView(sessionsWindow);
      listView->setModel(localModel);
      listView->setItemDelegate(new SessionDelegate(listView));
      connect(
          listView, &QListView::doubleClicked, this,
          [this, localModel](const QModelIndex &filterIndex) {
            QString sessId =
                localModel->data(filterIndex, SessionModel::IdRole).toString();
            m_apiManager->getSession(sessId);
            updateStatus(i18n("Fetching details for session %1...", sessId));
          });
      QMenu *fileMenu = new QMenu(i18n("File"), sessionsWindow);
      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), sessionsWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, sessionsWindow,
              &KXmlGuiWindow::close);
      fileMenu->addAction(closeAction);
      sessionsWindow->menuBar()->addMenu(fileMenu);

      sessionsWindow->setCentralWidget(listView);
      sessionsWindow->setupGUI();
      sessionsWindow->resize(600, 400);
      sessionsWindow->show();
    }
  });
  m_viewRawDataAction = new QAction(i18n("View Raw Data"), this);
  actionCollection()->addAction(QStringLiteral("view_raw_data"),
                                m_viewRawDataAction);
  connect(m_viewRawDataAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QJsonObject rawData =
          m_sourceModel->data(mappedIdx, SourceModel::RawDataRole)
              .toJsonObject();
      KXmlGuiWindow *rawWindow = new KXmlGuiWindow(this);
      rawWindow->setObjectName(
          QStringLiteral("RawDataWindow_%1")
              .arg(m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                       .toString()
                       .replace(QLatin1Char('/'), QLatin1Char('_'))));
      rawWindow->setAttribute(Qt::WA_DeleteOnClose);
      rawWindow->setWindowTitle(i18n("Raw Data for Source"));
      QTextBrowser *textBrowser = new QTextBrowser(rawWindow);
      QJsonDocument doc(rawData);
      textBrowser->setPlainText(
          QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

      QMenu *fileMenu = new QMenu(i18n("File"), rawWindow);
      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), rawWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, rawWindow,
              &KXmlGuiWindow::close);
      fileMenu->addAction(closeAction);
      rawWindow->menuBar()->addMenu(fileMenu);

      rawWindow->setCentralWidget(textBrowser);
      rawWindow->setupGUI();
      rawWindow->resize(600, 400);
      rawWindow->show();
    }
  });

  m_openUrlAction = new QAction(i18n("Open URL"), this);
  actionCollection()->addAction(QStringLiteral("open_url"), m_openUrlAction);
  connect(m_openUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

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
                   QStringLiteral(".com/") + owner + QLatin1Char('/') + repo;
        }
      } else {
        urlStr = id;
      }

      if (!urlStr.isEmpty()) {
        QDesktopServices::openUrl(QUrl(urlStr));
        count++;
      }
    }
    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(i18n("Invalid source ID for opening URL."));
    }
  });

  m_restoreDataAction = new QAction(i18n("Restore Data"), this);
  actionCollection()->addAction(QStringLiteral("restore_data"),
                                m_restoreDataAction);
  connect(m_restoreDataAction, &QAction::triggered, this,
          &MainWindow::restoreData);

  m_backupDataAction = new QAction(i18n("Backup Data"), this);
  actionCollection()->addAction(QStringLiteral("backup_data"),
                                m_backupDataAction);
  connect(m_backupDataAction, &QAction::triggered, this,
          &MainWindow::backupData);

  m_importTemplatesAction = new QAction(i18n("Import Templates..."), this);
  actionCollection()->addAction(QStringLiteral("import_templates"),
                                m_importTemplatesAction);
  connect(m_importTemplatesAction, &QAction::triggered, this,
          &MainWindow::importTemplates);

  m_exportTemplatesAction = new QAction(i18n("Export Templates..."), this);
  actionCollection()->addAction(QStringLiteral("export_templates"),
                                m_exportTemplatesAction);
  connect(m_exportTemplatesAction, &QAction::triggered, this,
          &MainWindow::exportTemplates);

  m_toggleQueueAction =
      new QAction(QIcon::fromTheme(QStringLiteral("media-playback-pause")),
                  i18n("Stop Queue"), this);
  actionCollection()->addAction(QStringLiteral("toggle_queue"),
                                m_toggleQueueAction);
  connect(m_toggleQueueAction, &QAction::triggered, this,
          &MainWindow::toggleQueueState);

  m_copyUrlAction = new QAction(i18n("Copy URL"), this);
  actionCollection()->addAction(QStringLiteral("copy_url"), m_copyUrlAction);
  connect(m_copyUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    QStringList urlsToCopy;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

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
                   QStringLiteral(".com/") + owner + QLatin1Char('/') + repo;
        }
      } else {
        urlStr = id;
      }

      if (!urlStr.isEmpty()) {
        urlsToCopy.append(urlStr);
      }
    }

    if (!urlsToCopy.isEmpty()) {
      QGuiApplication::clipboard()->setText(urlsToCopy.join(QLatin1Char('\n')));
      updateStatus(i18np("1 URL copied to clipboard.",
                         "%1 URLs copied to clipboard.", urlsToCopy.size()));
    } else {
      updateStatus(i18n("Invalid source ID for copying URL."));
    }
  });

  KStandardAction::preferences(this, &MainWindow::showSettingsDialog,
                               actionCollection());
  KStandardAction::configureToolbars(this, &KXmlGuiWindow::configureToolbars,
                                     actionCollection());
  KStandardAction::quit(qApp, &QCoreApplication::quit, actionCollection());

  setStandardToolBarMenuEnabled(true);

  // Set up XML GUI

  connect(m_sourcesFilterEditor, &FilterEditor::filterChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<AdvancedFilterProxyModel *>(
                    m_sourceView->model()))
              pm->setFilterQuery(text);
            else if (auto *pm = qobject_cast<QSortFilterProxyModel *>(
                         m_sourceView->model()))
              pm->setFilterFixedString(text);
          });
  connect(m_pastFilterEditor, &FilterEditor::filterChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<AdvancedFilterProxyModel *>(
                    m_sessionView->model()))
              pm->setFilterQuery(text);
          });
  connect(m_archiveFilterEditor, &FilterEditor::filterChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<AdvancedFilterProxyModel *>(
                    m_archiveView->model()))
              pm->setFilterQuery(text);
          });
  connect(m_draftsFilter, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<QSortFilterProxyModel *>(
                    m_draftsView->model())) {
              pm->setFilterCaseSensitivity(Qt::CaseInsensitive);
              pm->setFilterFixedString(text);
            }
          });
  connect(m_templatesFilter, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<QSortFilterProxyModel *>(
                    m_templatesView->model())) {
              pm->setFilterCaseSensitivity(Qt::CaseInsensitive);
              pm->setFilterFixedString(text);
            }
          });
  connect(m_errorsFilter, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (auto *pm = qobject_cast<QSortFilterProxyModel *>(
                    m_errorsView->model())) {
              pm->setFilterCaseSensitivity(Qt::CaseInsensitive);
              pm->setFilterFixedString(text);
            }
          });

  setupGUI(Default, QStringLiteral("kjulesui.rc"));

  if (auto *tb = toolBar(QStringLiteral("mainToolBar"))) {
    tb->show();
  }
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

  m_refreshSourcesAction->setText(i18n("Cancel Refresh"));
  m_sourceProgressBar->show();
  m_cancelRefreshBtn->show();

  updateStatus(i18n("Refreshing sources..."));
  m_apiManager->listSources();
}

void MainWindow::showNewSessionDialog() {
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  connect(&dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  connect(&dialog, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);
  dialog.exec();
}

void MainWindow::showSettingsDialog() {
  SettingsDialog dialog(m_apiManager, this);
  if (dialog.exec() == QDialog::Accepted) {
    loadQueueSettings();
  }
}

void MainWindow::loadQueueSettings() {
  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));
  int intervalMins = queueConfig.readEntry("TimerInterval", 1);
  m_queueTimer->setInterval(intervalMins * 60000);

  if (!m_queuePaused && !m_queueModel->isEmpty() && !m_queueTimer->isActive()) {
    m_queueTimer->start();
  }
}

void MainWindow::toggleQueueState() {
  m_queuePaused = !m_queuePaused;
  if (m_queuePaused) {
    m_queueTimer->stop();
    m_toggleQueueAction->setText(i18n("Process Queue"));
    m_toggleQueueAction->setIcon(
        QIcon::fromTheme(QStringLiteral("media-playback-start")));
    updateStatus(i18n("Queue processing paused."));
  } else {
    if (!m_queueModel->isEmpty()) {
      m_queueTimer->start();
      // Try processing immediately when unpaused
      QTimer::singleShot(0, this, &MainWindow::processQueue);
    }
    m_toggleQueueAction->setText(i18n("Stop Queue"));
    m_toggleQueueAction->setIcon(
        QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    updateStatus(i18n("Queue processing resumed."));
  }
}

void MainWindow::onSessionCreated(const QStringList &sources,
                                  const QString &prompt,
                                  const QString &automationMode,
                                  bool requirePlanApproval) {
  for (const QString &source : sources) {
    QJsonObject req;
    req[QStringLiteral("source")] = source;
    req[QStringLiteral("prompt")] = prompt;
    if (requirePlanApproval) {
      req[QStringLiteral("requirePlanApproval")] = true;
    }
    if (!automationMode.isEmpty()) {
      req[QStringLiteral("automationMode")] = automationMode;
    }
    m_queueModel->enqueue(req);
  }
  updateStatus(i18np("Added 1 task to queue.", "Added %1 tasks to queue.",
                     sources.size()));

  // Start timer if not running
  if (!m_queuePaused && !m_queueTimer->isActive()) {
    m_queueTimer->start();
  }

  // Trigger processing immediately if we can
  QTimer::singleShot(0, this, &MainWindow::processQueue);
}

void MainWindow::processQueue() {
  if (m_isProcessingQueue || m_queuePaused)
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

  QueueItem peekItem = m_queueModel->peek();
  if (peekItem.isWaitItem) {
    if (!peekItem.waitStartTime.isValid()) {
      peekItem.waitStartTime = QDateTime::currentDateTimeUtc();
      m_queueModel->updateItem(0, peekItem);
      QTimer::singleShot(peekItem.waitSeconds * 1000, this,
                         &MainWindow::processQueue);
    } else {
      qint64 elapsed =
          peekItem.waitStartTime.secsTo(QDateTime::currentDateTimeUtc());
      if (elapsed >= peekItem.waitSeconds) {
        m_queueModel->dequeue(); // Wait completed, remove wait item
        QTimer::singleShot(0, this, &MainWindow::processQueue);
      } else {
        QTimer::singleShot((peekItem.waitSeconds - elapsed) * 1000, this,
                           &MainWindow::processQueue);
      }
    }
    return;
  }

  m_isProcessingQueue = true;
  QueueItem item = m_queueModel->peek();
  m_apiManager->createSessionAsync(item.requestData);
}

void MainWindow::onSessionCreatedResult(bool success,
                                        const QJsonObject &session,
                                        const QString &errorMsg,
                                        const QString &rawResponse) {
  if (!m_isProcessingQueue) {
    if (success) {
      m_sessionModel->addSession(session);
      QString sourceId = session.value(QStringLiteral("sourceContext"))
                             .toObject()
                             .value(QStringLiteral("source"))
                             .toString();
      if (!sourceId.isEmpty())
        m_sourceModel->recordSessionCreated(sourceId);
      updateStatus(i18n("Session created successfully."));
    }
    return;
  }

  QueueItem item = m_queueModel->dequeue(); // Pop the item we were processing
  m_queueModel
      ->recordRun(); // Record that we completed a run successfully or not
  m_queueModel
      ->checkAndPrependDailyLimitWait(); // Dynamically check after a run
  m_isProcessingQueue = false;

  if (success) {
    m_sessionModel->addSession(session);
    QString sourceId = session.value(QStringLiteral("sourceContext"))
                           .toObject()
                           .value(QStringLiteral("source"))
                           .toString();
    if (!sourceId.isEmpty())
      m_sourceModel->recordSessionCreated(sourceId);
    updateStatus(i18n("Session created from queue."));
    m_queueBackoffUntil = QDateTime(); // reset backoff
    // The next item will be processed by the configured timer (m_queueTimer)
  } else {
    QJsonDocument errDoc = QJsonDocument::fromJson(rawResponse.toUtf8());
    bool isPrecondition = false;
    bool isResourceExhausted = false;
    if (errDoc.isObject()) {
      QJsonObject errObj =
          errDoc.object().value(QStringLiteral("error")).toObject();
      if (errObj.value(QStringLiteral("status")).toString() ==
          QStringLiteral("FAILED_PRECONDITION")) {
        isPrecondition = true;
      } else if (errObj.value(QStringLiteral("status")).toString() ==
                     QStringLiteral("RESOURCE_EXHAUSTED") ||
                 errObj.value(QStringLiteral("code")).toInt() == 429) {
        isResourceExhausted = true;
      }
    }

    if (isPrecondition) {
      updateStatus(
          i18n("Too many concurrent tasks, waiting before retrying..."));

      // Requeue the item without incrementing the error count
      m_queueModel->requeueTransient(item);

      // Apply a short backoff (e.g. 5 minutes)
      KConfigGroup queueConfig(KSharedConfig::openConfig(),
                               QStringLiteral("Queue"));
      int backoffMins = queueConfig.readEntry("PreconditionBackoffInterval", 5);
      m_queueBackoffUntil =
          QDateTime::currentDateTimeUtc().addSecs(backoffMins * 60);
    } else if (isResourceExhausted) {
      updateStatus(i18n("API Rate limit hit, adding a wait item..."));

      // Requeue the item without incrementing the error count
      m_queueModel->requeueTransient(item);

      // Prepend a wait item
      QueueItem waitItem;
      waitItem.isWaitItem = true;
      int delayMins = 60; // default 1 hour
      waitItem.waitSeconds = delayMins * 60;
      m_queueModel->prependWaitItem(waitItem);
      m_queueBackoffUntil = QDateTime(); // Clear backoff
    } else {
      updateStatus(i18n("Failed to create session from queue: %1", errorMsg));
      m_queueModel->requeueFailed(item, errorMsg, rawResponse);

      KConfigGroup queueConfig(KSharedConfig::openConfig(),
                               QStringLiteral("Queue"));
      int backoffMins = queueConfig.readEntry("BackoffInterval", 30);
      m_queueBackoffUntil =
          QDateTime::currentDateTimeUtc().addSecs(backoffMins * 60);
    }
  }
}

void MainWindow::onDraftSaved(const QJsonObject &draft) {
  m_draftsModel->addDraft(draft);
  updateStatus(i18n("Draft saved."));
}

void MainWindow::onTemplateSaved(const QJsonObject &tmpl) {
  m_templatesModel->addTemplate(tmpl);
  updateStatus(i18n("Template saved."));
}

void MainWindow::onTemplateActivated(const QModelIndex &index) {
  QJsonObject tmpl = m_templatesModel->getTemplate(index.row());

  // Create template dialog
  QJsonObject templateData = tmpl;

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  dialog.setInitialData(templateData);

  connect(&dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  connect(&dialog, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);

  dialog.exec();
}

void MainWindow::onQueueActivated(const QModelIndex &index) {
  if (!index.isValid())
    return;
  int row = index.row();
  QueueItem item = m_queueModel->getItem(row);
  if (item.errorCount > 0) {
    showErrorDetails(row);
  } else {
    editQueueItem(row);
  }
}

void MainWindow::onQueueContextMenu(const QPoint &pos) {
  QModelIndex index = m_queueView->indexAt(pos);
  if (!index.isValid())
    return;
  int row = index.row();

  QueueItem item = m_queueModel->getItem(row);

  QMenu menu;
  QAction *errorAction = nullptr;
  if (item.errorCount > 0) {
    errorAction =
        menu.addAction(QIcon::fromTheme(QStringLiteral("dialog-error")),
                       i18n("View Error Details"));
    menu.addSeparator();
  }

  QAction *editAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit"));
  QAction *deleteAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));
  QAction *draftAction =
      menu.addAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                     i18n("Convert to Draft"));
  QAction *copyTemplateAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy as Template"));
  QAction *sendAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("mail-send")), i18n("Send Now"));

  QAction *selected = menu.exec(m_queueView->viewport()->mapToGlobal(pos));
  if (errorAction && selected == errorAction) {
    showErrorDetails(row);
  } else if (selected == editAction) {
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
  } else if (selected == copyTemplateAction) {
    SaveDialog dlg(QStringLiteral("Template"), this);
    if (dlg.exec() == QDialog::Accepted) {
      QJsonObject data = item.requestData;
      data[QStringLiteral("name")] = dlg.nameOrComment();
      data[QStringLiteral("description")] = dlg.description();
      m_templatesModel->addTemplate(data);
      updateStatus(i18n("Template created from queued item."));
    }
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

void MainWindow::showErrorDetails(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.requestData.isEmpty())
    return;

  ErrorWindow *window = new ErrorWindow(row, item, this);
  connect(window, &ErrorWindow::editRequested, this,
          &MainWindow::editQueueItem);
  connect(window, &ErrorWindow::deleteRequested, this, [this](int r) {
    m_queueModel->removeItem(r);
    updateStatus(i18n("Task removed from queue."));
  });
  connect(window, &ErrorWindow::draftRequested, this,
          &MainWindow::convertQueueItemToDraft);
  connect(window, &ErrorWindow::templateRequested, this, [this, row]() {
    QueueItem item = m_queueModel->getItem(row);
    if (item.requestData.isEmpty())
      return;
    SaveDialog dlg(QStringLiteral("Template"), this);
    if (dlg.exec() == QDialog::Accepted) {
      QJsonObject data = item.requestData;
      data[QStringLiteral("name")] = dlg.nameOrComment();
      data[QStringLiteral("description")] = dlg.description();
      m_templatesModel->addTemplate(data);
      updateStatus(i18n("Template created from queued item."));
    }
  });
  connect(window, &ErrorWindow::sendNowRequested, this,
          &MainWindow::sendQueueItemNow);

  window->setAttribute(Qt::WA_DeleteOnClose);
  window->show();
}

void MainWindow::editQueueItem(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.requestData.isEmpty())
    return;

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  dialog.setEditMode(true);
  dialog.setInitialData(item.requestData);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, row](const QStringList &sources, const QString &p,
                      const QString &a, bool requirePlanApproval) {
            m_queueModel->removeItem(row);
            onSessionCreated(sources, p, a, requirePlanApproval);
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

void MainWindow::onSessionCreationFailed(const QJsonObject &request,
                                         const QJsonObject &response,
                                         const QString &errorString,
                                         const QString &httpDetails) {
  m_errorsModel->addError(request, response, errorString, httpDetails);
  updateStatus(i18n("Error saved."));
  int newRow = m_errorsModel->rowCount() - 1;

  ErrorWindow *window =
      new ErrorWindow(newRow, request,
                      QString::fromUtf8(QJsonDocument(response).toJson(
                          QJsonDocument::Indented)),
                      errorString, httpDetails, this);
  connect(window, &ErrorWindow::editRequested, [this](int row) {
    QModelIndex idx = m_errorsModel->index(row, 0);
    onErrorActivated(idx);
  });
  connect(window, &ErrorWindow::deleteRequested, [this](int row) {
    m_errorsModel->removeError(row);
    updateStatus(i18n("Error removed."));
  });
  connect(window, &ErrorWindow::draftRequested, [this](int row) {
    QJsonObject errData = m_errorsModel->getError(row);
    QJsonObject req = errData.value(QStringLiteral("request")).toObject();
    m_draftsModel->addDraft(req);
    m_errorsModel->removeError(row);
    updateStatus(i18n("Error converted to draft."));
  });
  connect(window, &ErrorWindow::templateRequested, [this](int row) {
    SaveDialog dlg(QStringLiteral("Template"), this);
    if (dlg.exec() == QDialog::Accepted) {
      QJsonObject errData = m_errorsModel->getError(row);
      QJsonObject req = errData.value(QStringLiteral("request")).toObject();
      req[QStringLiteral("name")] = dlg.nameOrComment();
      req[QStringLiteral("description")] = dlg.description();
      m_templatesModel->addTemplate(req);
      updateStatus(i18n("Template created from error item."));
    }
  });
  connect(window, &ErrorWindow::sendNowRequested, [this](int row) {
    QJsonObject errData = m_errorsModel->getError(row);
    QJsonObject req = errData.value(QStringLiteral("request")).toObject();
    m_errorsModel->removeError(row);
    m_apiManager->createSessionAsync(req);
    updateStatus(i18n("Sending error item immediately..."));
  });

  window->setAttribute(Qt::WA_DeleteOnClose);
  window->show();
}

void MainWindow::onErrorActivated(const QModelIndex &index) {
  QJsonObject errorData = m_errorsModel->getError(index.row());
  QJsonObject request = errorData.value(QStringLiteral("request")).toObject();

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  dialog.setInitialData(request);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, index](const QStringList &sources, const QString &p,
                        const QString &a, bool requirePlanApproval) {
            onSessionCreated(sources, p, a, requirePlanApproval);
            m_errorsModel->removeError(index.row());
          });

  connect(&dialog, &NewSessionDialog::saveDraftRequested,
          [this, index](const QJsonObject &d) {
            m_draftsModel->addDraft(d);
            m_errorsModel->removeError(index.row());
            updateStatus(i18n("Draft saved and error removed."));
          });

  dialog.exec();
}

void MainWindow::onDraftActivated(const QModelIndex &index) {
  QJsonObject draft = m_draftsModel->getDraft(index.row());
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  dialog.setInitialData(draft);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, index](const QStringList &sources, const QString &p,
                        const QString &a, bool requirePlanApproval) {
            onSessionCreated(sources, p, a, requirePlanApproval);
            m_draftsModel->removeDraft(index.row());
          });

  connect(&dialog, &NewSessionDialog::saveDraftRequested,
          [this, index](const QJsonObject &d) {
            m_draftsModel->removeDraft(index.row());
            m_draftsModel->addDraft(d);
            updateStatus(i18n("Draft updated."));
          });
  connect(&dialog, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);

  dialog.exec();
}

void MainWindow::onSourceActivated(const QModelIndex &index) {
  // Map index from proxy to source
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

  QJsonObject initData;
  QJsonArray sourcesArr;

  QModelIndexList selection = m_sourceView->selectionModel()->selectedIndexes();
  if (selection.isEmpty()) {
    selection.append(index);
  }

  for (const QModelIndex &selIndex : selection) {
    QModelIndex mappedIndex = proxy ? proxy->mapToSource(selIndex) : selIndex;
    QString srcName =
        m_sourceModel->data(mappedIndex, SourceModel::NameRole).toString();
    sourcesArr.append(srcName);
  }

  initData[QStringLiteral("sources")] = sourcesArr;

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  dialog.setInitialData(initData);

  connect(&dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  dialog.exec();
}

void MainWindow::connectSessionWindow(SessionWindow *window) {
  connect(window, &SessionWindow::templateRequested, this,
          [this](const QJsonObject &templateData) {
            SaveDialog dlg(QStringLiteral("Template"), this);
            if (dlg.exec() == QDialog::Accepted) {
              QJsonObject data = templateData;
              data[QStringLiteral("name")] = dlg.nameOrComment();
              data[QStringLiteral("description")] = dlg.description();
              m_templatesModel->addTemplate(data);
              updateStatus(i18n("Template saved."));
            }
          });

  connect(window, &SessionWindow::archiveRequested, this,
          [this, window](const QString &id) {
            for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
              if (m_sessionModel
                      ->data(m_sessionModel->index(i, 0), SessionModel::IdRole)
                      .toString() == id) {
                QJsonObject session = m_sessionModel->getSession(i);
                m_archiveModel->addSession(session);
                m_archiveModel->saveSessions();
                m_sessionModel->removeSession(i);
                updateStatus(i18n("Session archived."));
                window->close();
                break;
              }
            }
          });

  connect(window, &SessionWindow::deleteRequested, this,
          [this, window](const QString &id) {
            for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
              if (m_sessionModel
                      ->data(m_sessionModel->index(i, 0), SessionModel::IdRole)
                      .toString() == id) {
                m_sessionModel->removeSession(i);
                updateStatus(i18n("Session deleted."));
                window->close();
                break;
              }
            }
          });
}

void MainWindow::showSessionWindow(const QJsonObject &session) {
  QString sessionId = session.value(QStringLiteral("id")).toString();
  SessionWindow *window = new SessionWindow(
      session, m_apiManager, m_sessionModel->contains(sessionId), this);
  connect(window, &SessionWindow::watchRequested, this,
          [this](const QJsonObject &s) {
            m_sessionModel->addSession(s);
            m_sessionModel->saveSessions();
          });

  connectSessionWindow(window);
  window->show();
}

void MainWindow::onSessionActivated(const QModelIndex &index) {
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
  QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
  QJsonObject sessionData = m_sessionModel->getSession(sourceIndex.row());

  if (sessionData.isEmpty()) {
    QString id =
        m_sessionModel->data(sourceIndex, SessionModel::IdRole).toString();
    m_apiManager->getSession(id);
    updateStatus(i18n("Fetching details for session %1...", id));
  } else {
    QString sessionId = sessionData.value(QStringLiteral("id")).toString();
    SessionWindow *window = new SessionWindow(
        sessionData, m_apiManager, m_sessionModel->contains(sessionId), this);
    connect(window, &SessionWindow::watchRequested, this,
            [this](const QJsonObject &s) {
              m_sessionModel->addSession(s);
              m_sessionModel->saveSessions();
            });

    connectSessionWindow(window);
    window->show();
  }
}

void MainWindow::updateStatus(const QString &message) {
  m_statusLabel->setText(message);
  m_trayIcon->setToolTip(message);
}

void MainWindow::onError(const QString &message) {
  updateStatus(i18n("Error: %1", message));
  QMessageBox::critical(this, i18n("Error"), message);
}

void MainWindow::toggleWindow() { toggleWindowVisibility(); }

void MainWindow::toggleWindowVisibility() {
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

  if (!m_apiManager->githubToken().isEmpty()) {
    for (int i = 0; i < sources.size(); ++i) {
      QJsonObject source = sources[i].toObject();
      QString id = source.value(QStringLiteral("id")).toString();
      if (id.startsWith(QStringLiteral("sources/github/"))) {
        m_apiManager->fetchGithubInfo(id);
      }
    }
  }

  m_sourceProgressBar->setFormat(i18n("%1 sources loaded from %2 pages",
                                      m_sourcesLoadedCount,
                                      m_pagesLoadedCount));
  updateStatus(i18n("Loaded %1 sources, added %2 new.", m_sourcesLoadedCount,
                    m_sourcesAddedCount));
}

void MainWindow::onGithubInfoReceived(const QString &sourceId,
                                      const QJsonObject &info) {
  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex index = m_sourceModel->index(i, 0);
    QString id = m_sourceModel->data(index, SourceModel::IdRole).toString();
    if (id == sourceId) {
      QJsonObject source =
          m_sourceModel->data(index, SourceModel::RawDataRole).toJsonObject();
      source[QStringLiteral("github")] = info;
      m_sourceModel->updateSource(source);
      break;
    }
  }
}

void MainWindow::onSourcesRefreshFinished() {
  bool wasRefreshing = m_isRefreshingSources;
  m_isRefreshingSources = false;
  m_sourceProgressBar->hide();
  m_cancelRefreshBtn->hide();

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

void MainWindow::onSourceDetailsReceived(const QJsonObject &source) {
  m_sourceModel->updateSource(source);
  QString name = source.value(QStringLiteral("name")).toString();
  updateStatus(i18n("Source %1 refreshed successfully.", name));
}

void MainWindow::cancelSourcesRefresh() {
  m_apiManager->cancelListSources();
  m_isRefreshingSources = false;
  m_sourceProgressBar->hide();
  m_cancelRefreshBtn->hide();

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

void MainWindow::backupData() {
  QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(dataPath);

  BackupDialog dialog(dataPath, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  QString destDirStr = dialog.backupDirectory();
  QDir destDir(destDirStr);
  if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
    updateStatus(i18n("Failed to create backup directory: %1", destDirStr));
    return;
  }

  QString backupFileName = QStringLiteral("kjules_backup_%1.zip")
                               .arg(QDateTime::currentDateTime().toString(
                                   QStringLiteral("yyyyMMdd_HHmmss")));
  QString backupPath = destDir.filePath(backupFileName);

  KZip zip(backupPath);
  if (!zip.open(QIODevice::WriteOnly)) {
    updateStatus(i18n("Failed to create archive: %1", backupPath));
    return;
  }

  QStringList filesToBackup = dialog.filesToBackup();

  bool backedUpSomething = false;

  for (const QString &fileName : filesToBackup) {
    QString filePath = dataDir.filePath(fileName);
    QFile file(filePath);
    if (file.exists()) {
      zip.addLocalFile(filePath, fileName);
      backedUpSomething = true;
    }
  }

  zip.close();

  if (backedUpSomething) {
    updateStatus(i18n("Backup created at: %1", backupPath));
    if (dialog.removeOriginals()) {
      for (const QString &fileName : filesToBackup) {
        QString filePath = dataDir.filePath(fileName);
        QFile::remove(filePath);
      }

      // Clear models and UI since the underlying data has been removed
      if (filesToBackup.contains(QStringLiteral("sources.json")))
        m_sourceModel->clear();
      if (filesToBackup.contains(QStringLiteral("cached_sessions.json")) ||
          filesToBackup.contains(QStringLiteral("cached_all_sessions.json")))
        m_sessionModel->clear();
      if (filesToBackup.contains(
              QStringLiteral("cached_archive_sessions.json")))
        m_archiveModel->clear();
      if (filesToBackup.contains(QStringLiteral("drafts.json")))
        m_draftsModel->clear();
      if (filesToBackup.contains(QStringLiteral("queue.json")))
        m_queueModel->clear();
      if (filesToBackup.contains(QStringLiteral("errors.json")))
        m_errorsModel->clear();
      m_apiManager->cancelListSources(); // Cancel any ongoing fetch
    }

    if (dialog.openDirectory()) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(destDirStr));
    }
  } else {
    updateStatus(i18n("No files to backup."));
    QFile::remove(backupPath); // Delete the empty zip
  }
}

void MainWindow::restoreData() {
  QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  RestoreDialog dialog(dataPath, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  QString backupFilePath = dialog.restoreFile();
  if (backupFilePath.isEmpty()) {
    updateStatus(i18n("No backup file selected."));
    return;
  }

  KZip zip(backupFilePath);
  if (!zip.open(QIODevice::ReadOnly)) {
    updateStatus(i18n("Failed to open backup archive: %1", backupFilePath));
    return;
  }

  const KArchiveDirectory *archiveDir = zip.directory();
  if (!archiveDir) {
    updateStatus(i18n("Invalid archive structure."));
    return;
  }

  QStringList filesToRestore = dialog.filesToRestore();
  bool merge = dialog.mergeData();
  bool restoredSomething = false;
  QDir destDir(dataPath);

  for (const QString &fileName : filesToRestore) {
    const KArchiveEntry *entry = archiveDir->entry(fileName);
    if (!entry || !entry->isFile()) {
      continue;
    }

    const KArchiveFile *archiveFile = static_cast<const KArchiveFile *>(entry);
    QByteArray fileData = archiveFile->data();
    QString destPath = destDir.filePath(fileName);

    if (merge && QFile::exists(destPath)) {
      QFile existingFile(destPath);
      if (existingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument existingDoc =
            QJsonDocument::fromJson(existingFile.readAll());
        existingFile.close();
        QJsonDocument newDoc = QJsonDocument::fromJson(fileData);

        if (existingDoc.isArray() && newDoc.isArray()) {
          QJsonArray existingArray = existingDoc.array();
          QJsonArray newArray = newDoc.array();

          for (const QJsonValue &val : newArray) {
            if (!existingArray.contains(val)) {
              existingArray.append(val);
            }
          }

          QFile outFile(destPath);
          if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(QJsonDocument(existingArray).toJson());
            restoredSomething = true;
          }
        } else if (existingDoc.isObject() && newDoc.isObject()) {
          // Attempt merging objects based on a key (e.g., id)
          // Simplified fallback: Overwrite if complex merging isn't applicable
          QFile outFile(destPath);
          if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(fileData);
            restoredSomething = true;
          }
        } else {
          // Type mismatch or not JSON array/object, fallback to overwrite
          QFile outFile(destPath);
          if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(fileData);
            restoredSomething = true;
          }
        }
      }
    } else {
      // No merge, just overwrite/create
      QFile outFile(destPath);
      if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(fileData);
        restoredSomething = true;
      }
    }
  }

  zip.close();

  if (restoredSomething) {
    updateStatus(i18n("Data restored successfully. Data reloaded."));

    // Clear models so they reload properly
    if (filesToRestore.contains(QStringLiteral("sources.json"))) {
      m_sourceModel->clear();
      refreshSources();
    }
    if (filesToRestore.contains(QStringLiteral("cached_sessions.json")) ||
        filesToRestore.contains(QStringLiteral("cached_all_sessions.json"))) {
      m_sessionModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("drafts.json"))) {
      m_draftsModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("queue.json"))) {
      m_queueModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("errors.json"))) {
      m_errorsModel->clear();
    }
  } else {
    updateStatus(i18n("No files were restored."));
  }
}

void MainWindow::updateCompletions() {
  QMap<QString, QStringList> completions;
  completions[QStringLiteral("state")] =
      QStringList{QStringLiteral("RUNNING"),  QStringLiteral("QUEUED"),
                  QStringLiteral("PAUSED"),   QStringLiteral("ERROR"),
                  QStringLiteral("CANCELED"), QStringLiteral("DONE")};

  QStringList repos;
  QStringList owners;
  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex idx = m_sourceModel->index(i, SourceModel::ColName);
    repos.append(idx.data().toString());
  }
  repos.removeDuplicates();
  completions[QStringLiteral("repo")] = repos;
  completions[QStringLiteral("owner")] = repos; // just dummy for now

  m_sourcesFilterEditor->setCompletions(completions);
  m_pastFilterEditor->setCompletions(completions);
  m_archiveFilterEditor->setCompletions(completions);
}

void MainWindow::exportTemplates() {
  QString filePath = QFileDialog::getSaveFileName(
      this, i18n("Export Templates"), QString(), i18n("JSON Files (*.json)"));
  if (filePath.isEmpty())
    return;

  QJsonArray exportArray;
  for (int i = 0; i < m_templatesModel->rowCount(); ++i) {
    exportArray.append(m_templatesModel->getTemplate(i));
  }

  QJsonDocument doc(exportArray);
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    updateStatus(i18n("Templates exported to %1", filePath));
  } else {
    updateStatus(i18n("Failed to export templates to %1", filePath));
  }
}

void MainWindow::importTemplates() {
  QString filePath = QFileDialog::getOpenFileName(
      this, i18n("Import Templates"), QString(), i18n("JSON Files (*.json)"));
  if (filePath.isEmpty())
    return;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    updateStatus(i18n("Failed to open %1 for import", filePath));
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isArray()) {
    updateStatus(i18n("Invalid format: expected JSON array in %1", filePath));
    return;
  }

  QJsonArray importArray = doc.array();
  int importedCount = 0;
  for (const QJsonValue &val : importArray) {
    if (val.isObject()) {
      m_templatesModel->addTemplate(val.toObject());
      importedCount++;
    }
  }

  updateStatus(
      i18n("Imported %1 template(s) from %2", importedCount, filePath));
}

void MainWindow::copyTemplateToClipboard(const QModelIndex &index) {
  if (!index.isValid())
    return;
  QJsonArray exportArray;
  exportArray.append(m_templatesModel->getTemplate(index.row()));
  QJsonDocument doc(exportArray);
  QGuiApplication::clipboard()->setText(
      QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
  updateStatus(i18n("Template copied to clipboard."));
}

void MainWindow::pasteTemplateFromClipboard() {
  QString clipboardText = QGuiApplication::clipboard()->text();
  if (clipboardText.isEmpty()) {
    updateStatus(i18n("Clipboard is empty."));
    return;
  }
  QJsonDocument doc = QJsonDocument::fromJson(clipboardText.toUtf8());
  if (!doc.isArray() && !doc.isObject()) {
    updateStatus(i18n("Clipboard does not contain valid template JSON."));
    return;
  }
  int importedCount = 0;
  if (doc.isArray()) {
    QJsonArray importArray = doc.array();
    for (const QJsonValue &val : importArray) {
      if (val.isObject()) {
        m_templatesModel->addTemplate(val.toObject());
        importedCount++;
      }
    }
  } else if (doc.isObject()) {
    m_templatesModel->addTemplate(doc.object());
    importedCount = 1;
  }

  if (importedCount > 0) {
    updateStatus(
        i18n("Imported %1 template(s) from clipboard.", importedCount));
  } else {
    updateStatus(i18n("No templates found in clipboard JSON."));
  }
}
