#include "mainwindow.h"
#include "activitylogwindow.h"
#include "advancedfilterproxymodel.h"
#include "apimanager.h"
#include "backupdialog.h"
#include "blockedtreemodel.h"
#include "clickableprogressbar.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "errorsmodel.h"
#include "errorwindow.h"
#include "filtereditor.h"
#include "filterparser.h"
#include "followsessiondialog.h"
#include "newsessiondialog.h"
#include "queuedelegate.h"
#include "queuemodel.h"
#include "refreshprogresswindow.h"
#include "restoredialog.h"
#include "savedialog.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"
#include "settingsdialog.h"
#include "sourcemodel.h"
#include "sourcesrefreshprogresswindow.h"
#include "templateeditdialog.h"
#include "templatesmodel.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToolBar>
#include <KXMLGUIFactory>
#include <KZip>
#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
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
      m_queueModel(new QueueModel(this)),
      m_holdingModel(
          new QueueModel(this, QStringLiteral("holding.json"), true)),
      m_errorsModel(new ErrorsModel(this)), m_errorRetryTimer(new QTimer(this)),
      m_isRefreshingSources(false), m_sourcesLoadedCount(0),
      m_sourcesAddedCount(0), m_pagesLoadedCount(0),
      m_sessionRefreshTimer(new QTimer(this)),
      m_followingRefreshTimer(new QTimer(this)), m_queueTimer(new QTimer(this)),
      m_countdownTimer(new QTimer(this)), m_isProcessingQueue(false),
      m_queuePaused(false), m_refreshProgressWindow(nullptr) {
  setObjectName(QStringLiteral("MainWindow"));
  setupUi();

  connect(m_sessionRefreshTimer, &QTimer::timeout, this,
          &MainWindow::updateSessionStats);
  m_sessionRefreshTimer->start(60000); // 1 minute

  connect(m_followingRefreshTimer, &QTimer::timeout, this,
          &MainWindow::autoRefreshFollowing);

  connect(m_queueTimer, &QTimer::timeout, this, &MainWindow::processQueue);

  connect(m_countdownTimer, &QTimer::timeout, this,
          &MainWindow::updateCountdownStatus);
  m_countdownTimer->start(1000); // 1 second

  connect(m_errorRetryTimer, &QTimer::timeout, this,
          &MainWindow::processErrorRetries);
  m_errorRetryTimer->start(60000); // 1 minute

  loadQueueSettings();
  updateFollowingRefreshTimer();
  createActions();
  setupTrayIcon();

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sourcesReceived, this,
          &MainWindow::onSourcesReceived);
  connect(m_apiManager, &APIManager::sourcesRefreshFinished, this,
          &MainWindow::onSourcesRefreshFinished);
  connect(m_apiManager, &APIManager::githubInfoReceived, this,
          &MainWindow::onGithubInfoReceived);
  connect(m_apiManager, &APIManager::githubBranchesReceived, this,
          &MainWindow::onGithubBranchesReceived);
  connect(m_apiManager, &APIManager::githubPullRequestInfoReceived, this,
          &MainWindow::onGithubPullRequestInfoReceived);
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
            const QString id = session.value(QStringLiteral("id")).toString();
            const QString newState =
                session.value(QStringLiteral("state")).toString();
            const QString prevState = m_previousSessionStates.value(id);

            if (!prevState.isEmpty() && prevState != newState) {
              QString eventId;
              QString title;
              QString text;

              if (newState == QStringLiteral("DONE") &&
                  (prevState == QStringLiteral("RUNNING") ||
                   prevState == QStringLiteral("QUEUED"))) {
                eventId = QStringLiteral("followingSessionCompleted");
                title = i18n("Following Session Completed");
                text = i18n("Session %1 has completed.", id);
              } else if (newState == QStringLiteral("RUNNING") &&
                         prevState == QStringLiteral("QUEUED")) {
                // Following session requires attention (this represents
                // transitioning to in-progress) We'll use the
                // 'followingSessionRequiresAttention' event since moving to
                // progress often means we need to look at it
                eventId = QStringLiteral("followingSessionRequiresAttention");
                title = i18n("Following Session Requires Attention");
                text = i18n("Session %1 is now running.", id);
              } else if (newState == QStringLiteral("ERROR")) {
                eventId = QStringLiteral("followingSessionFailed");
                title = i18n("Following Session Failed");
                text = i18n("Session %1 has encountered an error.", id);
              }

              if (!eventId.isEmpty()) {
                KNotification *notification = new KNotification(
                    eventId, KNotification::CloseOnTimeout, this);
                notification->setTitle(title);
                notification->setText(text);
                connect(notification, &KNotification::closed, notification,
                        &QObject::deleteLater);

                auto actionHandler = [this]() {
                  if (m_tabWidget) {
                    for (int i = 0; i < m_tabWidget->count(); ++i) {
                      if (m_tabWidget->widget(i)->objectName() ==
                          QStringLiteral("followingTab")) {
                        m_tabWidget->setCurrentIndex(i);
                        break;
                      }
                    }
                  }
                };

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                notification->setDefaultAction(i18n("View"));
                connect(notification, &KNotification::defaultActivated, this,
                        actionHandler);
#else
            auto action = notification->addDefaultAction(i18n("View"));
            connect(action, &KNotificationAction::activated, this, actionHandler);
#endif
                notification->sendEvent();
              }
            }
            m_previousSessionStates[id] = newState;

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
  QTimer::singleShot(0, this, [this]() { checkAutoArchiveSessions(); });
}

void MainWindow::setMockApi(bool useMock) {
  if (useMock) {
    m_apiManager->setBaseUrl(QStringLiteral("http://localhost:8080/v1alpha"));
  }
}

MainWindow::~MainWindow() {}

static QSharedPointer<ASTNode> mergeFilterIntoAST(QSharedPointer<ASTNode> node,
                                                  const QString &type,
                                                  const QString &value,
                                                  bool isHide, bool &merged) {
  if (!node)
    return QSharedPointer<ASTNode>();

  if (isHide) {
    if (auto notNode = qSharedPointerDynamicCast<NotNode>(node)) {
      auto child = notNode->child();
      if (auto kvChild = qSharedPointerDynamicCast<KeyValueNode>(child)) {
        if (kvChild->key() == type) {
          QList<QSharedPointer<ASTNode>> orChildren;
          orChildren.append(child);
          orChildren.append(
              QSharedPointer<ASTNode>(new KeyValueNode(type, value)));
          merged = true;
          return QSharedPointer<ASTNode>(
              new NotNode(QSharedPointer<ASTNode>(new OrNode(orChildren))));
        }
      } else if (auto orChild = qSharedPointerDynamicCast<OrNode>(child)) {
        bool allSameType = true;
        for (const auto &c : orChild->children()) {
          auto kv = qSharedPointerDynamicCast<KeyValueNode>(c);
          if (!kv || kv->key() != type) {
            allSameType = false;
            break;
          }
        }
        if (allSameType) {
          QList<QSharedPointer<ASTNode>> newOrChildren = orChild->children();
          newOrChildren.append(
              QSharedPointer<ASTNode>(new KeyValueNode(type, value)));
          merged = true;
          return QSharedPointer<ASTNode>(
              new NotNode(QSharedPointer<ASTNode>(new OrNode(newOrChildren))));
        }
      }
    }
  } else {
    // If not hiding, we could try to merge into an existing AndNode, but
    // appending works fine. We only merge NOTs for neatness.
  }

  if (auto andNode = qSharedPointerDynamicCast<AndNode>(node)) {
    QList<QSharedPointer<ASTNode>> newChildren;
    for (const auto &child : andNode->children()) {
      if (!merged) {
        newChildren.append(
            mergeFilterIntoAST(child, type, value, isHide, merged));
      } else {
        newChildren.append(child);
      }
    }
    return QSharedPointer<ASTNode>(new AndNode(newChildren));
  } else if (auto orNode = qSharedPointerDynamicCast<OrNode>(node)) {
    QList<QSharedPointer<ASTNode>> newChildren;
    for (const auto &child : orNode->children()) {
      if (!merged) {
        newChildren.append(
            mergeFilterIntoAST(child, type, value, isHide, merged));
      } else {
        newChildren.append(child);
      }
    }
    return QSharedPointer<ASTNode>(new OrNode(newChildren));
  } else if (auto notNode = qSharedPointerDynamicCast<NotNode>(node)) {
    if (!merged) {
      auto newChild =
          mergeFilterIntoAST(notNode->child(), type, value, isHide, merged);
      return QSharedPointer<ASTNode>(new NotNode(newChild));
    }
  }

  return node;
}

void MainWindow::applyQuickFilter(FilterEditor *editor, const QString &type,
                                  const QString &value, bool isHide) {
  QString current = editor->filterText().trimmed();
  if (current.startsWith(QLatin1Char('='))) {
    current = current.mid(1).trimmed();
  }

  QSharedPointer<ASTNode> ast = FilterParser::parse(current);

  if (!ast || current.isEmpty()) {
    if (isHide) {
      ast = QSharedPointer<ASTNode>(
          new NotNode(QSharedPointer<ASTNode>(new KeyValueNode(type, value))));
    } else {
      ast = QSharedPointer<ASTNode>(new KeyValueNode(type, value));
    }
  } else {
    bool merged = false;
    ast = mergeFilterIntoAST(ast, type, value, isHide, merged);

    if (!merged) {
      QList<QSharedPointer<ASTNode>> andChildren;
      if (auto andNode = qSharedPointerDynamicCast<AndNode>(ast)) {
        andChildren = andNode->children();
      } else {
        andChildren.append(ast);
      }

      if (isHide) {
        andChildren.append(QSharedPointer<ASTNode>(new NotNode(
            QSharedPointer<ASTNode>(new KeyValueNode(type, value)))));
      } else {
        andChildren.append(
            QSharedPointer<ASTNode>(new KeyValueNode(type, value)));
      }
      ast = QSharedPointer<ASTNode>(new AndNode(andChildren));
    }
  }

  editor->setFilterText(QStringLiteral("=") + ast->toString());
}

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

void MainWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape && m_tabWidget) {
    QWidget *currentWidget = m_tabWidget->currentWidget();
    if (currentWidget) {
      if (QAbstractItemView *view =
              qobject_cast<QAbstractItemView *>(currentWidget)) {
        view->clearSelection();
      }
      const auto views = currentWidget->findChildren<QAbstractItemView *>();
      for (QAbstractItemView *view : views) {
        view->clearSelection();
      }
    }
  }
  KXmlGuiWindow::keyPressEvent(event);
}

QStringList MainWindow::getSelectedSessionIds() const {
  QModelIndexList selectedRows =
      m_sessionView->selectionModel()->selectedRows();
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
  QStringList ids;
  for (const QModelIndex &idx : selectedRows) {
    QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
    QString currentId =
        m_sessionModel->data(mappedIdx, SessionModel::IdRole).toString();
    if (!currentId.isEmpty()) {
      ids.append(currentId);
    }
  }
  return ids;
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);

  // Sources View
  QWidget *srcTab = new QWidget(this);
  srcTab->setObjectName(QStringLiteral("sourcesTab"));
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

  // Set default sorting to Favourites first
  m_sourceView->sortByColumn(SourceModel::ColName, Qt::AscendingOrder);
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
          QMenu *favMenu =
              menu.addMenu(QIcon::fromTheme(QStringLiteral("emblem-favorite")),
                           i18n("Favourite"));
          favMenu->addAction(m_toggleFavouriteAction);
          favMenu->addAction(i18n("Increase Rank"), this,
                             &MainWindow::increaseFavouriteRank);
          favMenu->addAction(i18n("Decrease Rank"), this,
                             &MainWindow::decreaseFavouriteRank);
          favMenu->addAction(i18n("Set Rank..."), this,
                             &MainWindow::setFavouriteRank);
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
          menu.addAction(m_showFollowingNewSessionsAction);
          menu.addAction(m_viewRawDataAction);
          menu.addAction(m_sourceSettingsAction);
          menu.addAction(m_openUrlAction);
          menu.addAction(m_copyUrlAction);

          menu.addSeparator();
          QAction *configLimitAction =
              menu.addAction(i18n("Configure Concurrency Limit"));
          connect(configLimitAction, &QAction::triggered, [this, id]() {
            KConfigGroup sourceConfig(KSharedConfig::openConfig(),
                                      QStringLiteral("SourceConcurrency"));
            int currentLimit =
                sourceConfig.readEntry(id, -1); // -1 means defer to global

            bool ok;
            int newLimit = QInputDialog::getInt(
                this, i18n("Concurrency Limit"),
                i18n("Enter max active sessions for %1 (0 to disable limit, -1 "
                     "to defer to global):",
                     id),
                currentLimit, -1, 1000, 1, &ok);
            if (ok) {
              if (newLimit == -1) {
                sourceConfig.deleteEntry(id);
              } else {
                sourceConfig.writeEntry(id, newLimit);
              }
              sourceConfig.sync();
              updateStatus(
                  i18n("Concurrency limit for %1 updated to %2", id, newLimit));
            }
          });

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
  QWidget *followingTab = new QWidget(this);
  followingTab->setObjectName(QStringLiteral("followingTab"));
  QVBoxLayout *followingLayout = new QVBoxLayout(followingTab);
  m_followingFilterEditor = new FilterEditor(this);
  followingLayout->addWidget(m_followingFilterEditor);

  m_sessionView = new QTreeView(this);
  followingLayout->addWidget(m_sessionView);

  AdvancedFilterProxyModel *sessionProxyModel =
      new AdvancedFilterProxyModel(this);
  sessionProxyModel->setSourceModel(m_sessionModel);
  m_sessionView->setModel(sessionProxyModel);
  m_sessionView->setSortingEnabled(true);
  m_sessionView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sessionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_sessionView->header()->setMinimumSectionSize(80);
  m_sessionView->header()->resizeSection(SessionModel::ColTitle,
                                         SessionModel::DefaultTitleWidth);
  m_sessionView->header()->setStretchLastSection(true);

  // Set default sorting to Title
  m_sessionView->sortByColumn(SessionModel::ColTitle, Qt::AscendingOrder);

  m_sessionView->setContextMenuPolicy(Qt::CustomContextMenu);

  auto deleteFollowingSessions = [this]() {
    QModelIndexList selectedRows =
        m_sessionView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    QList<int> rowsToDelete;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      if (!rowsToDelete.contains(mappedIdx.row())) {
        rowsToDelete.append(mappedIdx.row());
      }
    }
    std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

    for (int row : rowsToDelete) {
      m_sessionModel->removeSession(row);
    }
    updateStatus(i18np("1 session deleted.", "%1 sessions deleted.",
                       rowsToDelete.size()));
  };

  QAction *sessionDeleteAction = new QAction(i18n("Delete"), m_sessionView);
  sessionDeleteAction->setShortcut(QKeySequence::Delete);
  sessionDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(sessionDeleteAction, &QAction::triggered, deleteFollowingSessions);
  m_sessionView->addAction(sessionDeleteAction);

  connect(
      m_sessionView, &QTreeView::customContextMenuRequested,
      [this, deleteFollowingSessions](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        QMenu menu;
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

          QMenu *favMenu =
              menu.addMenu(QIcon::fromTheme(QStringLiteral("emblem-favorite")),
                           i18n("Favourite"));
          favMenu->addAction(m_toggleFavouriteAction);
          favMenu->addAction(i18n("Increase Rank"), this,
                             &MainWindow::increaseFavouriteRank);
          favMenu->addAction(i18n("Decrease Rank"), this,
                             &MainWindow::decreaseFavouriteRank);
          favMenu->addAction(i18n("Set Rank..."), this,
                             &MainWindow::setFavouriteRank);
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
          QAction *copyJulesIdAction = nullptr;
          if (!id.isEmpty()) {
            openJulesUrlAction = menu.addAction(i18n("Open Jules URL"));
            copyJulesUrlAction = menu.addAction(i18n("Copy Jules URL"));
            copyJulesIdAction = menu.addAction(i18n("Copy Jules ID"));
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

          QString owner = m_sessionModel
                              ->data(m_sessionModel->index(
                                  sourceIndex.row(), SessionModel::ColOwner))
                              .toString();
          QString repo = m_sessionModel
                             ->data(m_sessionModel->index(
                                 sourceIndex.row(), SessionModel::ColRepo))
                             .toString();
          if (!owner.isEmpty() && !repo.isEmpty()) {
            QAction *hideRepoAction = menu.addAction(i18n("Hide this repo"));
            QAction *hideOwnerAction = menu.addAction(i18n("Hide this owner"));
            QAction *onlyRepoAction = menu.addAction(i18n("Only this repo"));
            QAction *onlyOwnerAction = menu.addAction(i18n("Only this owner"));

            connect(hideRepoAction, &QAction::triggered, [this, repo]() {
              applyQuickFilter(m_followingFilterEditor, QStringLiteral("repo"),
                               repo, true);
            });
            connect(hideOwnerAction, &QAction::triggered, [this, owner]() {
              applyQuickFilter(m_followingFilterEditor, QStringLiteral("owner"),
                               owner, true);
            });
            connect(onlyRepoAction, &QAction::triggered, [this, repo]() {
              applyQuickFilter(m_followingFilterEditor, QStringLiteral("repo"),
                               repo, false);
            });
            connect(onlyOwnerAction, &QAction::triggered, [this, owner]() {
              applyQuickFilter(m_followingFilterEditor, QStringLiteral("owner"),
                               owner, false);
            });

            menu.addSeparator();
          }

          QAction *completeAction = menu.addAction(i18n("Mark as Complete"));
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
            QStringList idsToRefresh;
            for (const QModelIndex &idx : selectedRows) {
              QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
              QString currentId =
                  m_sessionModel->data(mappedIdx, SessionModel::IdRole)
                      .toString();
              if (!currentId.isEmpty()) {
                idsToRefresh.append(currentId);
              }
            }
            if (!idsToRefresh.isEmpty()) {
              if (m_refreshProgressWindow &&
                  !m_refreshProgressWindow->isFinishedProcess()) {
                m_refreshProgressWindow->addSessionIds(idsToRefresh);
                m_refreshProgressWindow->show();
                m_refreshProgressWindow->raise();
                m_refreshProgressWindow->activateWindow();
              } else {
                if (m_refreshProgressWindow) {
                  m_refreshProgressWindow->deleteLater();
                }
                m_refreshProgressWindow = new RefreshProgressWindow(
                    idsToRefresh, m_apiManager, m_sessionModel, this);
                connect(m_refreshProgressWindow,
                        &RefreshProgressWindow::progressUpdated, this,
                        &MainWindow::onRefreshProgressUpdated);
                connect(m_refreshProgressWindow,
                        &RefreshProgressWindow::progressFinished, this,
                        &MainWindow::onRefreshProgressFinished);
                connect(m_refreshProgressWindow,
                        &RefreshProgressWindow::openSessionRequested, this,
                        [this](const QString &id) {
                          m_apiManager->getSession(id);
                          updateStatus(
                              i18n("Fetching details for session %1...", id));
                        });

                m_sessionRefreshProgressBar->show();
                m_refreshProgressWindow->show();
              }
            } else {
              updateStatus(i18n("No following sessions to refresh."));
            }
          });

          if (openJulesUrlAction && copyJulesUrlAction && copyJulesIdAction) {
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
                      QStringLiteral("https://jules.google.com/session/") +
                      currentId;
                  QDesktopServices::openUrl(QUrl(urlStr));
                  count++;
                }
              }
              updateStatus(
                  i18np("Opened 1 session", "Opened %1 sessions", count));
            });
            connect(copyJulesUrlAction, &QAction::triggered, [this]() {
              QStringList ids = getSelectedSessionIds();
              QStringList urls;
              for (const QString &id : ids) {
                urls.append(
                    QStringLiteral("https://jules.google.com/session/") + id);
              }
              if (!urls.isEmpty()) {
                QGuiApplication::clipboard()->setText(
                    urls.join(QLatin1Char('\n')));
                updateStatus(i18np("1 Jules URL copied to clipboard.",
                                   "%1 Jules URLs copied to clipboard.",
                                   urls.size()));
              }
            });
            connect(copyJulesIdAction, &QAction::triggered, [this]() {
              QStringList ids = getSelectedSessionIds();
              if (!ids.isEmpty()) {
                QGuiApplication::clipboard()->setText(
                    ids.join(QLatin1Char('\n')));
                updateStatus(i18np("1 Jules ID copied to clipboard.",
                                   "%1 Jules IDs copied to clipboard.",
                                   ids.size()));
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

          auto archiveSelectedSessions = [this]() {
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
          };

          connect(completeAction, &QAction::triggered, archiveSelectedSessions);
          connect(archiveAction, &QAction::triggered, archiveSelectedSessions);

          connect(deleteAction, &QAction::triggered, deleteFollowingSessions);

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
                    auto window = new NewSessionDialog(
                        m_sourceModel, m_templatesModel, hasApiKey, this);
                    window->setInitialData(initData);
                    connect(window, &NewSessionDialog::refreshSourcesRequested,
                            this, &MainWindow::refreshSources);
                    connect(this, &MainWindow::statusMessage, window,
                            &NewSessionDialog::updateStatus);
                    connect(window, &NewSessionDialog::refreshGithubRequested,
                            this, &MainWindow::refreshGithubDataForSources);
                    connect(window, &NewSessionDialog::refreshSourceRequested,
                            this, [this](const QString &id) {
                              m_apiManager->getSource(id);
                            });
                    connect(window, &NewSessionDialog::createSessionRequested,
                            this, &MainWindow::onSessionCreated);
                    connect(window, &NewSessionDialog::saveDraftRequested, this,
                            &MainWindow::onDraftSaved);
                    window->show();
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
          menu.exec(m_sessionView->viewport()->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QTreeView::doubleClicked, this,
          &MainWindow::onSessionActivated);

  m_tabWidget->addTab(followingTab, i18n("Following"));
  // Archive View
  QWidget *archTab = new QWidget(this);
  archTab->setObjectName(QStringLiteral("archiveTab"));
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
  m_archiveView->header()->setMinimumSectionSize(80);
  m_archiveView->header()->resizeSection(SessionModel::ColTitle,
                                         SessionModel::DefaultTitleWidth);
  m_archiveView->header()->setStretchLastSection(true);

  // Set default sorting to Title
  m_archiveView->sortByColumn(SessionModel::ColTitle, Qt::AscendingOrder);

  m_archiveView->setContextMenuPolicy(Qt::CustomContextMenu);

  auto deleteArchiveSessions = [this]() {
    QModelIndexList selectedRows =
        m_archiveView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    QList<int> rowsToDelete;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_archiveView->model());
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      if (!rowsToDelete.contains(mappedIdx.row())) {
        rowsToDelete.append(mappedIdx.row());
      }
    }
    std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

    for (int row : rowsToDelete) {
      m_archiveModel->removeSession(row);
    }
    updateStatus(i18np("1 session deleted from archive.",
                       "%1 sessions deleted from archive.",
                       rowsToDelete.size()));
  };

  QAction *archiveDeleteAction = new QAction(i18n("Delete"), m_archiveView);
  archiveDeleteAction->setShortcut(QKeySequence::Delete);
  archiveDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(archiveDeleteAction, &QAction::triggered, deleteArchiveSessions);
  m_archiveView->addAction(archiveDeleteAction);

  connect(
      m_archiveView, &QTreeView::customContextMenuRequested,
      [this, deleteArchiveSessions](const QPoint &pos) {
        QModelIndex index = m_archiveView->indexAt(pos);
        QMenu menu;
        if (index.isValid()) {
          if (!m_archiveView->selectionModel()->isSelected(index)) {
            m_archiveView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect |
                           QItemSelectionModel::Rows);
            m_archiveView->setCurrentIndex(index);
          }
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(
                  m_archiveView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

          menu.addAction(m_toggleFavouriteAction);

          QMenu *favMenu =
              menu.addMenu(QIcon::fromTheme(QStringLiteral("emblem-favorite")),
                           i18n("Favourite"));
          favMenu->addAction(m_toggleFavouriteAction);
          favMenu->addAction(i18n("Increase Rank"), this,
                             &MainWindow::increaseFavouriteRank);
          favMenu->addAction(i18n("Decrease Rank"), this,
                             &MainWindow::decreaseFavouriteRank);
          favMenu->addAction(i18n("Set Rank..."), this,
                             &MainWindow::setFavouriteRank);
          QAction *openSessionAction = menu.addAction(i18n("Open Session"));
          QAction *unarchiveAction = menu.addAction(i18n("Unarchive"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));
          menu.addSeparator();

          QString owner = m_archiveModel
                              ->data(m_archiveModel->index(
                                  sourceIndex.row(), SessionModel::ColOwner))
                              .toString();
          QString repo = m_archiveModel
                             ->data(m_archiveModel->index(
                                 sourceIndex.row(), SessionModel::ColRepo))
                             .toString();
          if (!owner.isEmpty() && !repo.isEmpty()) {
            QAction *hideRepoAction = menu.addAction(i18n("Hide this repo"));
            QAction *hideOwnerAction = menu.addAction(i18n("Hide this owner"));
            QAction *onlyRepoAction = menu.addAction(i18n("Only this repo"));
            QAction *onlyOwnerAction = menu.addAction(i18n("Only this owner"));

            connect(hideRepoAction, &QAction::triggered, [this, repo]() {
              applyQuickFilter(m_archiveFilterEditor, QStringLiteral("repo"),
                               repo, true);
            });
            connect(hideOwnerAction, &QAction::triggered, [this, owner]() {
              applyQuickFilter(m_archiveFilterEditor, QStringLiteral("owner"),
                               owner, true);
            });
            connect(onlyRepoAction, &QAction::triggered, [this, repo]() {
              applyQuickFilter(m_archiveFilterEditor, QStringLiteral("repo"),
                               repo, false);
            });
            connect(onlyOwnerAction, &QAction::triggered, [this, owner]() {
              applyQuickFilter(m_archiveFilterEditor, QStringLiteral("owner"),
                               owner, false);
            });

            menu.addSeparator();
          }

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

          connect(deleteAction, &QAction::triggered, deleteArchiveSessions);

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
          menu.exec(m_archiveView->viewport()->mapToGlobal(pos));
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

  auto deleteDrafts = [this]() {
    QModelIndexList selectedRows =
        m_draftsView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    if (QMessageBox::question(
            this, i18np("Delete Draft", "Delete Drafts", selectedRows.size()),
            i18np("Are you sure?",
                  "Are you sure you want to delete these drafts?",
                  selectedRows.size())) == QMessageBox::Yes) {
      QList<int> rowsToDelete;
      for (const QModelIndex &idx : selectedRows) {
        if (!rowsToDelete.contains(idx.row())) {
          rowsToDelete.append(idx.row());
        }
      }
      std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

      for (int row : rowsToDelete) {
        m_draftsModel->removeDraft(row);
      }
    }
  };

  QAction *draftsDeleteAction = new QAction(i18n("Delete"), m_draftsView);
  draftsDeleteAction->setShortcut(QKeySequence::Delete);
  draftsDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(draftsDeleteAction, &QAction::triggered, deleteDrafts);
  m_draftsView->addAction(draftsDeleteAction);

  connect(m_draftsView, &QListView::customContextMenuRequested,
          [this, deleteDrafts](const QPoint &pos) {
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
                updateStatus(i18np("1 draft duplicated.",
                                   "%1 drafts duplicated.",
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

              connect(deleteAction, &QAction::triggered, deleteDrafts);

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

  auto deleteTemplates = [this]() {
    QModelIndexList selectedRows =
        m_templatesView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    if (QMessageBox::question(
            this,
            i18np("Delete Template", "Delete Templates", selectedRows.size()),
            i18np("Are you sure you want to delete this template?",
                  "Are you sure you want to delete these templates?",
                  selectedRows.size())) == QMessageBox::Yes) {
      QList<int> rowsToDelete;
      for (const QModelIndex &idx : selectedRows) {
        if (!rowsToDelete.contains(idx.row())) {
          rowsToDelete.append(idx.row());
        }
      }
      std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

      for (int row : rowsToDelete) {
        m_templatesModel->removeTemplate(row);
      }
      updateStatus(i18np("1 template deleted.", "%1 templates deleted.",
                         selectedRows.size()));
    }
  };

  QAction *templatesDeleteAction = new QAction(i18n("Delete"), m_templatesView);
  templatesDeleteAction->setShortcut(QKeySequence::Delete);
  templatesDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(templatesDeleteAction, &QAction::triggered, deleteTemplates);
  m_templatesView->addAction(templatesDeleteAction);

  connect(
      m_templatesView, &QListView::customContextMenuRequested,
      [this, deleteTemplates](const QPoint &pos) {
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
          connect(deleteAction, &QAction::triggered, deleteTemplates);
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
  m_queueView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_queueView->setDragDropMode(QAbstractItemView::DragDrop);
  m_queueView->setDragEnabled(true);
  m_queueView->setAcceptDrops(true);
  m_queueView->setDropIndicatorShown(true);
  m_queueView->setDefaultDropAction(Qt::MoveAction);
  connect(m_queueView, &QListView::activated, this,
          &MainWindow::onQueueActivated);
  connect(m_queueView, &QListView::customContextMenuRequested, this,
          &MainWindow::onQueueContextMenu);

  m_deleteQueueItemsLambda = [this]() {
    QModelIndexList selectedRows =
        m_queueView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    if (QMessageBox::question(this, i18n("Remove Task"),
                              i18np("Remove this task from the queue?",
                                    "Remove these tasks from the queue?",
                                    selectedRows.size())) == QMessageBox::Yes) {
      QList<int> rowsToDelete;
      for (const QModelIndex &idx : selectedRows) {
        if (!rowsToDelete.contains(idx.row())) {
          rowsToDelete.append(idx.row());
        }
      }
      std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

      for (int row : rowsToDelete) {
        m_queueModel->removeItem(row);
      }
      updateStatus(i18np("Task removed from queue.",
                         "%1 tasks removed from queue.", rowsToDelete.size()));
    }
  };

  QAction *queueDeleteAction = new QAction(i18n("Delete"), m_queueView);
  queueDeleteAction->setShortcut(QKeySequence::Delete);
  queueDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(queueDeleteAction, &QAction::triggered, m_deleteQueueItemsLambda);
  m_queueView->addAction(queueDeleteAction);

  m_tabWidget->addTab(m_queueView, i18n("Queue"));

  m_holdingView = new QListView(this);
  m_holdingView->setModel(m_holdingModel);
  m_holdingView->setItemDelegate(new QueueDelegate(this));
  m_holdingView->setContextMenuPolicy(Qt::CustomContextMenu);
  m_holdingView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_holdingView->setDragDropMode(QAbstractItemView::DragDrop);
  m_holdingView->setDragEnabled(true);
  m_holdingView->setAcceptDrops(true);
  m_holdingView->setDropIndicatorShown(true);
  m_holdingView->setDefaultDropAction(Qt::MoveAction);
  connect(m_holdingView, &QListView::activated, this,
          &MainWindow::onHoldingActivated);
  connect(m_holdingView, &QListView::customContextMenuRequested, this,
          &MainWindow::onHoldingContextMenu);

  m_deleteHoldingItemsLambda = [this]() {
    QModelIndexList selectedRows =
        m_holdingView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    if (QMessageBox::question(
            this, i18n("Remove Task"),
            i18np("Remove this task from the holding queue?",
                  "Remove these tasks from the holding queue?",
                  selectedRows.size())) == QMessageBox::Yes) {
      QList<int> rowsToDelete;
      for (const QModelIndex &idx : selectedRows) {
        if (!rowsToDelete.contains(idx.row())) {
          rowsToDelete.append(idx.row());
        }
      }
      std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

      for (int row : rowsToDelete) {
        m_holdingModel->removeItem(row);
      }
      updateStatus(i18np("Task removed from holding queue.",
                         "%1 tasks removed from holding queue.",
                         rowsToDelete.size()));
    }
  };

  QAction *holdingDeleteAction = new QAction(i18n("Delete"), m_holdingView);
  holdingDeleteAction->setShortcut(QKeySequence::Delete);
  holdingDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(holdingDeleteAction, &QAction::triggered, m_deleteHoldingItemsLambda);
  m_holdingView->addAction(holdingDeleteAction);

  connect(m_holdingModel, &QAbstractListModel::rowsInserted, this,
          &MainWindow::updateHoldingTabVisibility);
  connect(m_holdingModel, &QAbstractListModel::rowsRemoved, this,
          &MainWindow::updateHoldingTabVisibility);
  connect(m_holdingModel, &QAbstractListModel::modelReset, this,
          &MainWindow::updateHoldingTabVisibility);
  updateHoldingTabVisibility();

  // Blocked View
  m_blockedTreeModel = new BlockedTreeModel(m_sourceModel, m_queueModel, this);
  m_blockedView = new QTreeView(this);
  m_blockedView->setModel(m_blockedTreeModel);
  m_blockedView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_blockedView, &QTreeView::customContextMenuRequested, this,
          &MainWindow::onBlockedContextMenu);

  connect(m_blockedTreeModel, &QAbstractItemModel::rowsInserted, this,
          &MainWindow::updateBlockedTabVisibility);
  connect(m_blockedTreeModel, &QAbstractItemModel::rowsRemoved, this,
          &MainWindow::updateBlockedTabVisibility);
  connect(m_blockedTreeModel, &QAbstractItemModel::modelReset, this,
          &MainWindow::updateBlockedTabVisibility);
  updateBlockedTabVisibility();

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

  auto deleteErrors = [this]() {
    QModelIndexList selectedRows =
        m_errorsView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    if (QMessageBox::question(
            this, i18np("Delete Error", "Delete Errors", selectedRows.size()),
            i18np("Are you sure?",
                  "Are you sure you want to delete these errors?",
                  selectedRows.size())) == QMessageBox::Yes) {
      QList<int> rowsToDelete;
      for (const QModelIndex &idx : selectedRows) {
        if (!rowsToDelete.contains(idx.row())) {
          rowsToDelete.append(idx.row());
        }
      }
      std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

      for (int row : rowsToDelete) {
        m_errorsModel->removeError(row);
      }
    }
  };

  QAction *errorsDeleteAction = new QAction(i18n("Delete"), m_errorsView);
  errorsDeleteAction->setShortcut(QKeySequence::Delete);
  errorsDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(errorsDeleteAction, &QAction::triggered, deleteErrors);
  m_errorsView->addAction(errorsDeleteAction);

  connect(
      m_errorsView, &QListView::customContextMenuRequested,
      [this, deleteErrors](const QPoint &pos) {
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
          QAction *requeueAction = menu.addAction(i18n("Requeue"));
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

          connect(requeueAction, &QAction::triggered, [this]() {
            QModelIndexList selectedRows =
                m_errorsView->selectionModel()->selectedRows();
            QList<int> rowsToRequeue;
            for (const QModelIndex &idx : selectedRows) {
              if (!rowsToRequeue.contains(idx.row())) {
                rowsToRequeue.append(idx.row());
              }
            }
            std::sort(rowsToRequeue.begin(), rowsToRequeue.end(),
                      std::greater<int>());

            for (int row : rowsToRequeue) {
              QJsonObject errData = m_errorsModel->getError(row);
              QJsonObject req =
                  errData.value(QStringLiteral("request")).toObject();
              m_queueModel->enqueue(req);
              m_errorsModel->removeError(row);
            }
            if (!rowsToRequeue.isEmpty()) {
              updateStatus(i18np("Requeued 1 error item.",
                                 "Requeued %1 error items.",
                                 rowsToRequeue.size()));
              if (!m_queuePaused && !m_queueTimer->isActive()) {
                m_queueTimer->start();
              }
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
              connect(window, &ErrorWindow::requeueRequested, [this](int row) {
                QJsonObject errData = m_errorsModel->getError(row);
                QJsonObject req =
                    errData.value(QStringLiteral("request")).toObject();
                m_errorsModel->removeError(row);
                m_queueModel->enqueue(req);
                updateStatus(i18n("Error item requeued."));
                if (!m_queuePaused && !m_queueTimer->isActive()) {
                  m_queueTimer->start();
                }
              });

              connect(window, &ErrorWindow::requeueRequested, [this](int row) {
                QJsonObject errData = m_errorsModel->getError(row);
                QJsonObject req =
                    errData.value(QStringLiteral("request")).toObject();
                QueueItem item;
                item.requestData = req;
                if (errData.contains(QStringLiteral("pastErrors"))) {
                  item.pastErrors =
                      errData.value(QStringLiteral("pastErrors")).toArray();
                }
                QJsonObject strippedError = errData;
                strippedError.remove(QStringLiteral("pastErrors"));
                item.pastErrors.append(strippedError);
                m_queueModel->enqueueItem(item);
                m_errorsModel->removeError(row);
                updateStatus(i18n("Error requeued."));
              });

              window->setAttribute(Qt::WA_DeleteOnClose);
              window->show();
            }
          });

          connect(deleteAction, &QAction::triggered, deleteErrors);

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

  m_sourcesRefreshProgressWindow = nullptr;
  m_sourceProgressBar = new ClickableProgressBar(this);
  m_sourceProgressBar->hide();
  connect(m_sourceProgressBar, &ClickableProgressBar::clicked, this, [this]() {
    if (m_sourcesRefreshProgressWindow) {
      m_sourcesRefreshProgressWindow->show();
      m_sourcesRefreshProgressWindow->raise();
      m_sourcesRefreshProgressWindow->activateWindow();
    }
  });
  statusBar()->addPermanentWidget(m_sourceProgressBar);

  m_sessionRefreshProgressBar = new ClickableProgressBar(this);
  m_sessionRefreshProgressBar->hide();
  connect(m_sessionRefreshProgressBar, &ClickableProgressBar::clicked, this,
          &MainWindow::onSessionRefreshProgressBarClicked);
  statusBar()->addPermanentWidget(m_sessionRefreshProgressBar);

  m_cancelRefreshBtn = new QPushButton(i18n("Cancel"), this);
  m_cancelRefreshBtn->hide();
  connect(m_cancelRefreshBtn, &QPushButton::clicked, this,
          &MainWindow::cancelSourcesRefresh);
  statusBar()->addPermanentWidget(m_cancelRefreshBtn);

  // Connect model signals to update tab titles with counts
  connectModelForTabUpdates(m_draftsModel);
  connectModelForTabUpdates(m_templatesModel);
  connectModelForTabUpdates(m_errorsModel);
  connectModelForTabUpdates(m_queueModel);
  connectModelForTabUpdates(m_sessionModel);

  // Initial title update
  updateTabTitles();
}

void MainWindow::onRefreshProgressUpdated(int current, int total) {
  m_sessionRefreshProgressBar->setMaximum(total);
  m_sessionRefreshProgressBar->setValue(current);
  m_sessionRefreshProgressBar->setFormat(
      i18n("Refreshing %1/%2", qMin(current + 1, total), total));
}

void MainWindow::onRefreshProgressFinished() {
  m_sessionRefreshProgressBar->hide();
}

void MainWindow::onSessionRefreshProgressBarClicked() {
  if (m_refreshProgressWindow) {
    m_refreshProgressWindow->show();
    m_refreshProgressWindow->raise();
    m_refreshProgressWindow->activateWindow();
  }
}

void MainWindow::connectModelForTabUpdates(QAbstractItemModel *model) {
  connect(model, &QAbstractItemModel::rowsInserted, this,
          &MainWindow::updateTabTitles);
  connect(model, &QAbstractItemModel::rowsRemoved, this,
          &MainWindow::updateTabTitles);
  connect(model, &QAbstractItemModel::modelReset, this,
          &MainWindow::updateTabTitles);
}

void MainWindow::checkAutoArchiveSessions() {
  KConfigGroup sessionConfig(KSharedConfig::openConfig(),
                             QStringLiteral("SessionWindow"));
  bool autoArchiveEnabled = sessionConfig.readEntry("AutoArchiveEnabled", true);
  int autoArchiveDays = sessionConfig.readEntry("AutoArchiveDays", 30);
  bool prMergeArchiveEnabled =
      sessionConfig.readEntry("PrMergeArchiveEnabled", true);

  QDateTime now = QDateTime::currentDateTimeUtc();

  struct ArchiveItem {
    int row;
    QString id;
    QString reason;
  };
  QList<ArchiveItem> itemsToArchive;

  for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
    QJsonObject session = m_sessionModel->getSession(i);
    QString sessionId = session.value(QStringLiteral("id")).toString();
    bool shouldArchive = false;
    QString archiveReason;

    if (autoArchiveEnabled) {
      QString createTimeStr =
          session.value(QStringLiteral("createTime")).toString();
      QDateTime createTime =
          QDateTime::fromString(createTimeStr, Qt::ISODate).toUTC();
      if (createTime.isValid() && createTime.daysTo(now) >= autoArchiveDays) {
        shouldArchive = true;
        archiveReason =
            i18np("Older than 1 day", "Older than %1 days", autoArchiveDays);
      }
    }

    if (!shouldArchive && prMergeArchiveEnabled) {
      if (session.contains(QStringLiteral("githubPrInfo"))) {
        QJsonObject prInfo =
            session.value(QStringLiteral("githubPrInfo")).toObject();
        if (prInfo.value(QStringLiteral("state")).toString() ==
                QStringLiteral("merged") ||
            prInfo.value(QStringLiteral("merged_at")).isString()) {
          shouldArchive = true;
          archiveReason = i18n("PR is merged");
        }
      }
    }

    if (shouldArchive) {
      itemsToArchive.append({i, sessionId, archiveReason});
    }
  }

  for (const ArchiveItem &item : itemsToArchive) {
    QJsonObject session = m_sessionModel->getSession(item.row);
    m_archiveModel->addSession(session);
    m_sessionModel->removeSession(item.row);
    Q_EMIT sessionAutoArchived(item.id, item.reason);
  }

  if (!itemsToArchive.isEmpty()) {
    m_archiveModel->saveSessions();
    m_sessionModel->saveSessions();
    ActivityLogWindow::instance()->logMessage(
        i18np("Auto-archived 1 following session.",
              "Auto-archived %1 following sessions.", itemsToArchive.size()));
  }
}

void MainWindow::updateTabTitles() {
  if (!m_tabWidget)
    return;

  for (int i = 0; i < m_tabWidget->count(); ++i) {
    QWidget *page = m_tabWidget->widget(i);
    if (page == m_draftsView->parentWidget()) {
      int count = m_draftsModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Drafts (%1)", count)
                                           : i18n("Drafts"));
    } else if (page == m_templatesView->parentWidget()) {
      int count = m_templatesModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Templates (%1)", count)
                                           : i18n("Templates"));
    } else if (page == m_errorsView->parentWidget()) {
      int count = m_errorsModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Errors (%1)", count)
                                           : i18n("Errors"));
    } else if (page == m_queueView) {
      int count = m_queueModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Queue (%1)", count)
                                           : i18n("Queue"));
    } else if (page == m_sessionView->parentWidget()) {
      int count = m_sessionModel->rowCount();
      m_tabWidget->setTabText(i, count > 0 ? i18n("Following (%1)", count)
                                           : i18n("Following"));
    }
  }
}

void MainWindow::onGithubPullRequestInfoReceived(const QString &prUrl,
                                                 const QJsonObject &info) {
  QString newPrState = info.value(QStringLiteral("state")).toString();

  for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
    QModelIndex index = m_sessionModel->index(i, 0);
    if (m_sessionModel->data(index, SessionModel::PrUrlRole).toString() ==
        prUrl) {
      QJsonObject session = m_sessionModel->getSession(i);
      QString id = session.value(QStringLiteral("id")).toString();

      QString prevPrState = m_previousSessionPrStates.value(id);
      if (!prevPrState.isEmpty() && prevPrState != newPrState) {
        if (newPrState == QStringLiteral("open")) {
          KNotification *notification =
              new KNotification(QStringLiteral("followingSessionNewPROpened"),
                                KNotification::CloseOnTimeout, this);
          notification->setTitle(i18n("Following Session New PR Opened"));
          notification->setText(i18n("Session %1 has a new PR opened.", id));
          connect(notification, &KNotification::closed, notification,
                  &QObject::deleteLater);

          auto actionHandler = [this]() {
            if (m_tabWidget) {
              for (int i = 0; i < m_tabWidget->count(); ++i) {
                if (m_tabWidget->widget(i)->objectName() ==
                    QStringLiteral("followingTab")) {
                  m_tabWidget->setCurrentIndex(i);
                  break;
                }
              }
            }
          };

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
          notification->setDefaultAction(i18n("View"));
          connect(notification, &KNotification::defaultActivated, this,
                  actionHandler);
#else
          auto action = notification->addDefaultAction(i18n("View"));
          connect(action, &KNotificationAction::activated, this, actionHandler);
#endif
          notification->sendEvent();
        }
      }
      m_previousSessionPrStates[id] = newPrState;

      session[QStringLiteral("githubPrInfo")] = info;
      m_sessionModel->updateSession(session);
      // Also update archive model and watch model if needed.
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

  checkAutoArchiveSessions();
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon(QStringLiteral(":/icons/kjules-tray.png")));
  m_trayIcon->setToolTip(i18n("Google Jules Client"));

  m_trayMenu = new QMenu(this);

  QAction *showHideAction =
      new QAction(i18n("Show/Hide (Suggest %1)",
                       QKeySequence(Qt::META | Qt::Key_J).toString()),
                  this);
  connect(showHideAction, &QAction::triggered, this,
          &MainWindow::toggleWindowVisibility);
  m_trayMenu->addAction(showHideAction);

  m_trayMenu->addSeparator();

  QAction *newSessionAction = new QAction(i18n("New Session"), this);
  connect(newSessionAction, &QAction::triggered, this,
          [this]() { showNewSessionDialog(); });
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
          [this]() { showNewSessionDialog(); });
  actionCollection()->addAction(QStringLiteral("new_session"),
                                newSessionAction);
  KGlobalAccel::setGlobalShortcut(
      newSessionAction, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_J));
  actionCollection()->setDefaultShortcut(newSessionAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_N));

  m_showActivityLogAction = new QAction(i18n("Show Activity Log"), this);
  actionCollection()->addAction(QStringLiteral("show_activity_log"),
                                m_showActivityLogAction);
  connect(m_showActivityLogAction, &QAction::triggered, this, []() {
    ActivityLogWindow::instance()->show();
    ActivityLogWindow::instance()->raise();
    ActivityLogWindow::instance()->activateWindow();
  });

  m_toggleFavouriteAction =
      new QAction(QIcon::fromTheme(QStringLiteral("emblem-favorite")),
                  i18n("Toggle Favourite"), this);
  connect(m_toggleFavouriteAction, &QAction::triggered, this,
          &MainWindow::toggleFavourite);

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

  m_followFromIdAction =
      new QAction(i18n("Follow from Jules Session ID"), this);
  actionCollection()->addAction(QStringLiteral("follow_from_id"),
                                m_followFromIdAction);
  connect(m_followFromIdAction, &QAction::triggered, this, [this]() {
    FollowSessionDialog dialog(m_apiManager, this);
    if (dialog.exec() == QDialog::Accepted) {
      QString id = dialog.sessionId();
      if (!id.isEmpty()) {
        QJsonObject session = dialog.sessionData();
        if (!session.isEmpty()) {
          m_sessionModel->addSession(session);
          m_sessionModel->saveSessions();
          updateStatus(i18n("Started following session %1", id));
        } else {
          QMetaObject::Connection *connection = new QMetaObject::Connection;
          *connection = connect(
              m_apiManager, &APIManager::sessionDetailsReceived, this,
              [this, id, connection](const QJsonObject &session) {
                if (session.value(QStringLiteral("id")).toString() == id) {
                  m_sessionModel->addSession(session);
                  m_sessionModel->saveSessions();
                  updateStatus(i18n("Started following session %1", id));
                  disconnect(*connection);
                  delete connection;
                }
              });

          // Also clean up on error to prevent memory leaks
          QMetaObject::Connection *errorConnection =
              new QMetaObject::Connection;
          *errorConnection = connect(m_apiManager, &APIManager::errorOccurred,
                                     this, [connection, errorConnection]() {
                                       disconnect(*connection);
                                       delete connection;
                                       disconnect(*errorConnection);
                                       delete errorConnection;
                                     });

          m_apiManager->getSession(id);
        }
      }
    }
  });

  m_sourceSettingsAction = new QAction(i18n("Source Settings"), this);
  actionCollection()->addAction(QStringLiteral("source_settings"),
                                m_sourceSettingsAction);
  connect(m_sourceSettingsAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    if (selectedRows.size() > 0) {
      QModelIndex idx = selectedRows.first();
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QMainWindow *settingsWindow = new QMainWindow(this);
      settingsWindow->setObjectName(
          QStringLiteral("SourceSettingsWindow_%1")
              .arg(m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                       .toString()
                       .replace(QLatin1Char('/'), QLatin1Char('_'))));
      settingsWindow->setAttribute(Qt::WA_DeleteOnClose);
      settingsWindow->setWindowTitle(i18n("Source Settings"));

      QTextEdit *textEdit = new QTextEdit(settingsWindow);
      QJsonObject rawData =
          m_sourceModel->data(mappedIdx, SourceModel::RawDataRole)
              .toJsonObject();
      QJsonDocument doc(rawData);
      textEdit->setPlainText(
          QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

      QMenu *fileMenu = new QMenu(i18n("File"), settingsWindow);
      QAction *saveAction =
          new QAction(QIcon::fromTheme(QStringLiteral("document-save")),
                      i18n("Save"), settingsWindow);
      saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
      connect(saveAction, &QAction::triggered, settingsWindow,
              [this, textEdit, mappedIdx, settingsWindow]() {
                QJsonParseError parseError;
                QJsonDocument newDoc = QJsonDocument::fromJson(
                    textEdit->toPlainText().toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError) {
                  QMessageBox::warning(settingsWindow, i18n("Invalid JSON"),
                                       i18n("The JSON data is invalid: %1",
                                            parseError.errorString()));
                  return;
                }
                QJsonObject mergedData = newDoc.object();

                m_sourceModel->updateSource(mergedData);
                updateStatus(i18n("Source settings saved successfully."));
              });
      fileMenu->addAction(saveAction);

      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), settingsWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, settingsWindow,
              &QMainWindow::close);
      fileMenu->addAction(closeAction);
      settingsWindow->menuBar()->addMenu(fileMenu);

      settingsWindow->setCentralWidget(textEdit);
      settingsWindow->resize(600, 400);
      settingsWindow->show();
    }
  });

  m_refreshSourcesAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sources"), this);
  connect(m_refreshSourcesAction, &QAction::triggered, this,
          &MainWindow::refreshSources);
  actionCollection()->addAction(QStringLiteral("refresh_sources"),
                                m_refreshSourcesAction);

  QAction *refreshCurrentTabAction =
      new QAction(i18n("Refresh Current Tab"), this);
  actionCollection()->setDefaultShortcut(refreshCurrentTabAction,
                                         QKeySequence(Qt::Key_F5));
  connect(refreshCurrentTabAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() ==
          QStringLiteral("sourcesTab")) {
        m_refreshSourcesAction->trigger();
      } else if (m_tabWidget->currentWidget()->objectName() ==
                 QStringLiteral("followingTab")) {
        m_refreshFollowingAction->trigger();
      }
    }
  });
  actionCollection()->addAction(QStringLiteral("refresh_current_tab"),
                                refreshCurrentTabAction);

  m_refreshFollowingAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Following"), this);
  connect(m_refreshFollowingAction, &QAction::triggered, this, [this]() {
    m_sessionModel->clearAllUnreadChanges();
    QStringList idsToRefresh;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
      QModelIndex index = m_sessionModel->index(i, 0);
      QString currentId =
          m_sessionModel->data(index, SessionModel::IdRole).toString();
      if (!currentId.isEmpty()) {
        idsToRefresh.append(currentId);
      }
    }
    if (!idsToRefresh.isEmpty()) {
      if (m_refreshProgressWindow &&
          !m_refreshProgressWindow->isFinishedProcess()) {
        m_refreshProgressWindow->addSessionIds(idsToRefresh);
        m_refreshProgressWindow->show();
        m_refreshProgressWindow->raise();
        m_refreshProgressWindow->activateWindow();
      } else {
        if (m_refreshProgressWindow) {
          m_refreshProgressWindow->deleteLater();
        }
        m_refreshProgressWindow = new RefreshProgressWindow(
            idsToRefresh, m_apiManager, m_sessionModel, this);
        connect(m_refreshProgressWindow,
                &RefreshProgressWindow::progressUpdated, this,
                &MainWindow::onRefreshProgressUpdated);
        connect(m_refreshProgressWindow,
                &RefreshProgressWindow::progressFinished, this,
                &MainWindow::onRefreshProgressFinished);
        connect(m_refreshProgressWindow,
                &RefreshProgressWindow::openSessionRequested, this,
                [this](const QString &id) {
                  m_apiManager->getSession(id);
                  updateStatus(i18n("Fetching details for session %1...", id));
                });

        m_sessionRefreshProgressBar->show();
        m_refreshProgressWindow->show();
      }
    } else {
      updateStatus(i18n("No following sessions to refresh."));
    }
  });
  actionCollection()->addAction(QStringLiteral("refresh_following"),
                                m_refreshFollowingAction);

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
                                         QKeySequence(Qt::META | Qt::Key_J));

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

  m_showFollowingNewSessionsAction =
      new QAction(i18n("Show following new sessions"), this);
  actionCollection()->addAction(QStringLiteral("show_following_new_sessions"),
                                m_showFollowingNewSessionsAction);
  connect(
      m_showFollowingNewSessionsAction, &QAction::triggered, this, [this]() {
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
            QString sessionSource =
                session.value(QStringLiteral("sourceContext"))
                    .toObject()
                    .value(QStringLiteral("source"))
                    .toString();
            if (sessionSource == id) {
              filteredSessions.append(session);
            }
          }
          QMainWindow *sessionsWindow = new QMainWindow(this);
          sessionsWindow->setObjectName(
              QStringLiteral("FollowingNewSessions_%1").arg(id));
          SessionModel *localModel = new SessionModel(
              QStringLiteral("cached_all_sessions.json"), sessionsWindow);
          localModel->setSessions(filteredSessions);
          sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
          sessionsWindow->setWindowTitle(
              i18n("Following New Sessions for %1", id));
          QListView *listView = new QListView(sessionsWindow);
          listView->setModel(localModel);
          listView->setItemDelegate(new SessionDelegate(listView));
          connect(listView, &QListView::doubleClicked, this,
                  [this, localModel](const QModelIndex &filterIndex) {
                    QString sessId =
                        localModel->data(filterIndex, SessionModel::IdRole)
                            .toString();
                    m_apiManager->getSession(sessId);
                    updateStatus(
                        i18n("Fetching details for session %1...", sessId));
                  });
          QMenu *fileMenu = new QMenu(i18n("File"), sessionsWindow);
          QAction *closeAction =
              new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                          i18n("Close"), sessionsWindow);
          closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
          connect(closeAction, &QAction::triggered, sessionsWindow,
                  &QMainWindow::close);
          fileMenu->addAction(closeAction);
          sessionsWindow->menuBar()->addMenu(fileMenu);

          sessionsWindow->setCentralWidget(listView);
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
      QMainWindow *rawWindow = new QMainWindow(this);
      rawWindow->setObjectName(
          QStringLiteral("RawDataWindow_%1")
              .arg(m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                       .toString()
                       .replace(QLatin1Char('/'), QLatin1Char('_'))));
      rawWindow->setAttribute(Qt::WA_DeleteOnClose);
      rawWindow->setWindowTitle(i18n("Raw Data for Source"));
      QTabWidget *tabWidget = new QTabWidget(rawWindow);

      QTextBrowser *sourceBrowser = new QTextBrowser(tabWidget);
      QJsonDocument doc(rawData);
      sourceBrowser->setPlainText(
          QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
      tabWidget->addTab(sourceBrowser, i18n("Source Data"));

      if (rawData.contains(QStringLiteral("github"))) {
        QTextBrowser *githubBrowser = new QTextBrowser(tabWidget);
        QJsonDocument githubDoc(
            rawData.value(QStringLiteral("github")).toObject());
        githubBrowser->setPlainText(
            QString::fromUtf8(githubDoc.toJson(QJsonDocument::Indented)));
        tabWidget->addTab(githubBrowser, i18n("GitHub API Data"));
      }

      QMenu *fileMenu = new QMenu(i18n("File"), rawWindow);
      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), rawWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, rawWindow, &QMainWindow::close);
      fileMenu->addAction(closeAction);
      rawWindow->menuBar()->addMenu(fileMenu);

      rawWindow->setCentralWidget(tabWidget);
      rawWindow->resize(600, 400);
      rawWindow->show();
    }
  });

  m_openJulesUrlAction = new QAction(i18n("Open Jules URL"), this);
  actionCollection()->addAction(QStringLiteral("open_jules_url"),
                                m_openJulesUrlAction);
  connect(m_openJulesUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sessionView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sessionModel->data(mappedIdx, SessionModel::IdRole).toString();

      if (!id.isEmpty()) {
        QString urlStr =
            QStringLiteral("https://jules.google.com/session/") + id;
        QDesktopServices::openUrl(QUrl(urlStr));
        count++;
      }
    }
    if (count > 0) {
      updateStatus(
          i18np("Opened 1 Jules URL.", "Opened %1 Jules URLs.", count));
    } else {
      updateStatus(i18n("Invalid session ID for opening Jules URL."));
    }
  });

  m_openGithubUrlAction = new QAction(i18n("Open Github URL"), this);
  actionCollection()->addAction(QStringLiteral("open_github_url"),
                                m_openGithubUrlAction);
  connect(m_openGithubUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sessionView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString prUrl =
          m_sessionModel->data(mappedIdx, SessionModel::PrUrlRole).toString();

      if (!prUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(prUrl));
        count++;
      }
    }
    if (count > 0) {
      updateStatus(
          i18np("Opened 1 Github URL.", "Opened %1 Github URLs.", count));
    } else {
      updateStatus(i18n("No Github PR URLs found for selected sessions."));
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

  m_archiveMergedFollowingAction = new QAction(
      i18n("Archive all Following items that are in \"PR merged\" state"),
      this);
  actionCollection()->addAction(QStringLiteral("archive_merged_following"),
                                m_archiveMergedFollowingAction);
  connect(m_archiveMergedFollowingAction, &QAction::triggered, this, [this]() {
    int count = 0;
    for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
      if (m_sessionModel
              ->data(m_sessionModel->index(i, 0), SessionModel::PrStatusRole)
              .toString() == QStringLiteral("merged")) {
        QJsonObject session = m_sessionModel->getSession(i);
        m_archiveModel->addSession(session);
        m_sessionModel->removeSession(i);
        count++;
      }
    }
    if (count > 0) {
      m_archiveModel->saveSessions();
      m_sessionModel->saveSessions();
      updateStatus(
          i18np("1 session archived.", "%1 sessions archived.", count));
    } else {
      updateStatus(i18n("No sessions in \"PR merged\" state found."));
    }
  });

  m_archivePausedFollowingAction = new QAction(
      i18n("Archive all Following items that are in \"Paused\" state"), this);
  actionCollection()->addAction(QStringLiteral("archive_paused_following"),
                                m_archivePausedFollowingAction);
  connect(m_archivePausedFollowingAction, &QAction::triggered, this, [this]() {
    int count = 0;
    for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
      if (m_sessionModel
              ->data(m_sessionModel->index(i, 0), SessionModel::StateRole)
              .toString() == QStringLiteral("PAUSED")) {
        QJsonObject session = m_sessionModel->getSession(i);
        m_archiveModel->addSession(session);
        m_sessionModel->removeSession(i);
        count++;
      }
    }
    if (count > 0) {
      m_archiveModel->saveSessions();
      m_sessionModel->saveSessions();
      updateStatus(
          i18np("1 session archived.", "%1 sessions archived.", count));
    } else {
      updateStatus(i18n("No sessions in \"Paused\" state found."));
    }
  });

  m_archiveFailedFollowingAction = new QAction(
      i18n("Archive all Following items that are in \"Failed\" state"), this);
  actionCollection()->addAction(QStringLiteral("archive_failed_following"),
                                m_archiveFailedFollowingAction);
  connect(m_archiveFailedFollowingAction, &QAction::triggered, this, [this]() {
    int count = 0;
    for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
      if (m_sessionModel
              ->data(m_sessionModel->index(i, 0), SessionModel::StateRole)
              .toString() == QStringLiteral("ERROR")) {
        QJsonObject session = m_sessionModel->getSession(i);
        m_archiveModel->addSession(session);
        m_sessionModel->removeSession(i);
        count++;
      }
    }
    if (count > 0) {
      m_archiveModel->saveSessions();
      m_sessionModel->saveSessions();
      updateStatus(
          i18np("1 session archived.", "%1 sessions archived.", count));
    } else {
      updateStatus(i18n("No sessions in \"Failed\" state found."));
    }
  });

  m_archiveCompletedFollowingAction = new QAction(
      i18n("Archive all Following items that are in \"Completed\" state"),
      this);
  actionCollection()->addAction(QStringLiteral("archive_completed_following"),
                                m_archiveCompletedFollowingAction);
  connect(
      m_archiveCompletedFollowingAction, &QAction::triggered, this, [this]() {
        int count = 0;
        for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
          if (m_sessionModel
                  ->data(m_sessionModel->index(i, 0), SessionModel::StateRole)
                  .toString() == QStringLiteral("DONE")) {
            QJsonObject session = m_sessionModel->getSession(i);
            m_archiveModel->addSession(session);
            m_sessionModel->removeSession(i);
            count++;
          }
        }
        if (count > 0) {
          m_archiveModel->saveSessions();
          m_sessionModel->saveSessions();
          updateStatus(
              i18np("1 session archived.", "%1 sessions archived.", count));
        } else {
          updateStatus(i18n("No sessions in \"Completed\" state found."));
        }
      });

  m_archiveCanceledFollowingAction = new QAction(
      i18n("Archive all Following items that are in \"Canceled\" state"), this);
  actionCollection()->addAction(QStringLiteral("archive_canceled_following"),
                                m_archiveCanceledFollowingAction);
  connect(
      m_archiveCanceledFollowingAction, &QAction::triggered, this, [this]() {
        int count = 0;
        for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
          if (m_sessionModel
                  ->data(m_sessionModel->index(i, 0), SessionModel::StateRole)
                  .toString() == QStringLiteral("CANCELED")) {
            QJsonObject session = m_sessionModel->getSession(i);
            m_archiveModel->addSession(session);
            m_sessionModel->removeSession(i);
            count++;
          }
        }
        if (count > 0) {
          m_archiveModel->saveSessions();
          m_sessionModel->saveSessions();
          updateStatus(
              i18np("1 session archived.", "%1 sessions archived.", count));
        } else {
          updateStatus(i18n("No sessions in \"Canceled\" state found."));
        }
      });

  m_duplicateFailedToQueueAndArchiveAction =
      new QAction(i18n("Duplicate all Following items that are in \"Failed\" "
                       "state to queue and archive"),
                  this);
  actionCollection()->addAction(
      QStringLiteral("duplicate_failed_to_queue_and_archive"),
      m_duplicateFailedToQueueAndArchiveAction);
  connect(m_duplicateFailedToQueueAndArchiveAction, &QAction::triggered, this,
          [this]() {
            duplicateFollowingItemsToQueue(QStringLiteral("ERROR"),
                                           i18n("Failed"));
          });

  m_duplicatePausedToQueueAndArchiveAction =
      new QAction(i18n("Duplicate all Following items that are in \"Paused\" "
                       "state to queue and archive"),
                  this);
  actionCollection()->addAction(
      QStringLiteral("duplicate_paused_to_queue_and_archive"),
      m_duplicatePausedToQueueAndArchiveAction);
  connect(m_duplicatePausedToQueueAndArchiveAction, &QAction::triggered, this,
          [this]() {
            duplicateFollowingItemsToQueue(QStringLiteral("PAUSED"),
                                           i18n("Paused"));
          });

  m_duplicateCanceledToQueueAndArchiveAction =
      new QAction(i18n("Duplicate all Following items that are in \"Canceled\" "
                       "state to queue and archive"),
                  this);
  actionCollection()->addAction(
      QStringLiteral("duplicate_canceled_to_queue_and_archive"),
      m_duplicateCanceledToQueueAndArchiveAction);
  connect(m_duplicateCanceledToQueueAndArchiveAction, &QAction::triggered, this,
          [this]() {
            duplicateFollowingItemsToQueue(QStringLiteral("CANCELED"),
                                           i18n("Canceled"));
          });

  m_purgeArchiveAction = new QAction(i18n("Purge archive"), this);
  actionCollection()->addAction(QStringLiteral("purge_archive"),
                                m_purgeArchiveAction);
  connect(m_purgeArchiveAction, &QAction::triggered, this, [this]() {
    int count = m_archiveModel->rowCount();
    if (count > 0) {
      if (QMessageBox::question(
              this, i18n("Purge Archive"),
              i18np("Are you sure you want to purge the archive (1 session)?",
                    "Are you sure you want to purge the archive (%1 sessions)?",
                    count)) == QMessageBox::Yes) {
        m_archiveModel->clearSessions();
        m_archiveModel->saveSessions();
        updateStatus(i18np("Archive purged (1 session removed).",
                           "Archive purged (%1 sessions removed).", count));
      }
    } else {
      updateStatus(i18n("Archive is already empty."));
    }
  });

  m_configureConcurrencyLimitAction =
      new QAction(i18n("Configure Concurrency Limit..."), this);
  actionCollection()->addAction(QStringLiteral("configure_concurrency_limit"),
                                m_configureConcurrencyLimitAction);
  connect(
      m_configureConcurrencyLimitAction, &QAction::triggered, this, [this]() {
        QModelIndexList selectedRows =
            m_sourceView->selectionModel()->selectedRows();
        if (selectedRows.isEmpty())
          return;
        const QSortFilterProxyModel *proxy =
            qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
        QModelIndex mappedIdx = proxy ? proxy->mapToSource(selectedRows.first())
                                      : selectedRows.first();
        QString id =
            m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

        KConfigGroup sourceConfig(KSharedConfig::openConfig(),
                                  QStringLiteral("SourceConcurrency"));
        int currentLimit = sourceConfig.readEntry(id, -1);

        bool ok;
        int newLimit =
            QInputDialog::getInt(this, i18n("Concurrency Limit"),
                                 i18n("Enter max active sessions for %1 (0 to "
                                      "disable limit, -1 to defer to global):",
                                      id),
                                 currentLimit, -1, 1000, 1, &ok);
        if (ok) {
          if (newLimit == -1) {
            sourceConfig.deleteEntry(id);
          } else {
            sourceConfig.writeEntry(id, newLimit);
          }
          sourceConfig.sync();
          updateStatus(
              i18n("Concurrency limit for %1 updated to %2", id, newLimit));
          QTimer::singleShot(0, this, &MainWindow::processQueue);
        }
      });

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
  KStandardAction::close(this, &QWidget::close, actionCollection());
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
  connect(m_followingFilterEditor, &FilterEditor::filterChanged, this,
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

  m_favouritesMenu = qobject_cast<QMenu *>(
      factory()->container(QStringLiteral("favourites"), this));
  if (m_favouritesMenu) {
    connect(m_favouritesMenu, &QMenu::aboutToShow, this,
            &MainWindow::updateFavouritesMenu);
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

  if (!m_sourcesRefreshProgressWindow) {
    m_sourcesRefreshProgressWindow =
        new SourcesRefreshProgressWindow(m_apiManager, this);
    connect(m_sourcesRefreshProgressWindow,
            &SourcesRefreshProgressWindow::progressSummary, this,
            &MainWindow::updateStatus);
  }
  m_sourcesRefreshProgressWindow->reset();

  updateStatus(i18n("Refreshing sources..."));
  m_apiManager->listSources();
}

void MainWindow::showNewSessionDialog(const QJsonObject &initialData,
                                      bool ignoreSelection) {
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(window, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  connect(window, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);

  QJsonObject finalData = initialData;
  if (!ignoreSelection && finalData.isEmpty() && isActiveWindow() &&
      m_tabWidget && m_tabWidget->currentWidget() &&
      m_tabWidget->currentWidget()->objectName() ==
          QStringLiteral("sourcesTab")) {
    QModelIndexList selection = m_sourceView->selectionModel()->selectedRows();
    if (!selection.isEmpty()) {
      QJsonArray sourcesArr;
      const QSortFilterProxyModel *proxy =
          qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
      for (const QModelIndex &selIndex : selection) {
        QModelIndex mappedIndex =
            proxy ? proxy->mapToSource(selIndex) : selIndex;
        QString srcName =
            m_sourceModel->data(mappedIndex, SourceModel::NameRole).toString();
        sourcesArr.append(srcName);
      }
      finalData[QStringLiteral("sources")] = sourcesArr;
    }
  }

  if (!finalData.isEmpty()) {
    window->setInitialData(finalData);
  }
  window->show();
}

void MainWindow::showSettingsDialog() {
  SettingsDialog dialog(m_apiManager, this);
  if (dialog.exec() == QDialog::Accepted) {
    loadQueueSettings();
    updateFollowingRefreshTimer();
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

void MainWindow::updateFollowingRefreshTimer() {
  KConfigGroup sessionConfig(KSharedConfig::openConfig(),
                             QStringLiteral("SessionWindow"));
  int seconds = sessionConfig.readEntry("FollowingAutoRefreshInterval", 0);

  if (seconds > 0) {
    m_followingRefreshTimer->start(seconds * 1000);
  } else {
    m_followingRefreshTimer->stop();
  }
}

void MainWindow::updateCountdownStatus() {
  // Inform the queue model to update display of active wait items
  m_queueModel->refreshWaitItems();

  if (m_queuePaused || m_queueModel->isEmpty()) {
    return;
  }

  QDateTime now = QDateTime::currentDateTimeUtc();
  qint64 secondsLeft = 0;

  if (m_queueBackoffUntil.isValid() && now < m_queueBackoffUntil) {
    secondsLeft = now.secsTo(m_queueBackoffUntil);
  } else {
    QueueItem peekItem = m_queueModel->peek();
    if (peekItem.isWaitItem && peekItem.waitStartTime.isValid()) {
      qint64 elapsed = peekItem.waitStartTime.secsTo(now);
      if (elapsed < peekItem.waitSeconds) {
        secondsLeft = peekItem.waitSeconds - elapsed;
      }
    } else if (!m_isProcessingQueue) {
      if (m_queueTimer->isActive()) {
        secondsLeft = m_queueTimer->remainingTime() / 1000;
      }
    }
  }

  if (secondsLeft > 0) {
    QString timeStr;
    if (secondsLeft > 3600) {
      qint64 hours = secondsLeft / 3600;
      qint64 mins = (secondsLeft % 3600) / 60;
      timeStr = i18n("%1h %2m", hours, mins);
    } else if (secondsLeft > 60) {
      qint64 mins = secondsLeft / 60;
      qint64 secs = secondsLeft % 60;
      timeStr = i18n("%1m %2s", mins, secs);
    } else {
      timeStr = i18np("1 second", "%1 seconds", secondsLeft);
    }
    updateStatus(i18n("Next attempt in %1...", timeStr));
  }
}

void MainWindow::duplicateFollowingItemsToQueue(const QString &targetState,
                                                const QString &stateName) {
  int count = 0;
  for (int i = m_sessionModel->rowCount() - 1; i >= 0; --i) {
    if (m_sessionModel
            ->data(m_sessionModel->index(i, 0), SessionModel::StateRole)
            .toString() == targetState) {
      QJsonObject session = m_sessionModel->getSession(i);

      QJsonObject req;
      req[QStringLiteral("source")] =
          session.value(QStringLiteral("sourceContext"))
              .toObject()
              .value(QStringLiteral("source"))
              .toString();
      req[QStringLiteral("prompt")] =
          session.value(QStringLiteral("prompt")).toString();
      if (session.contains(QStringLiteral("automationMode"))) {
        req[QStringLiteral("automationMode")] =
            session.value(QStringLiteral("automationMode")).toString();
      }

      m_queueModel->enqueue(req);

      m_archiveModel->addSession(session);
      m_sessionModel->removeSession(i);
      count++;
    }
  }
  if (count > 0) {
    m_archiveModel->saveSessions();
    m_sessionModel->saveSessions();
    updateStatus(i18np("1 session duplicated to queue and archived.",
                       "%1 sessions duplicated to queue and archived.", count));
  } else {
    updateStatus(i18n("No sessions in \"%1\" state found.", stateName));
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

void MainWindow::onSessionCreated(const QMultiMap<QString, QString> &sources,
                                  const QString &prompt,
                                  const QString &automationMode,
                                  bool requirePlanApproval) {
  for (auto it = sources.begin(); it != sources.end(); ++it) {
    QJsonObject req;
    req[QStringLiteral("source")] = it.key();
    req[QStringLiteral("startingBranch")] = it.value();
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

void MainWindow::processErrorRetries() {
  QDateTime now = QDateTime::currentDateTimeUtc();
  QList<int> rowsToRetry;

  for (int i = m_errorsModel->rowCount() - 1; i >= 0; --i) {
    QJsonObject error = m_errorsModel->getError(i);
    QString timestampStr = error.value(QStringLiteral("timestamp")).toString();
    if (!timestampStr.isEmpty()) {
      QDateTime timestamp = QDateTime::fromString(timestampStr, Qt::ISODate);
      int errorCount = 1;
      if (error.contains(QStringLiteral("pastErrors"))) {
        errorCount +=
            error.value(QStringLiteral("pastErrors")).toArray().size();
      }
      qint64 backoffSecs = QueueModel::calculateBackoff(errorCount);
      if (timestamp.isValid() && timestamp.secsTo(now) >= backoffSecs) {
        rowsToRetry.append(i);
      }
    }
  }

  for (int row : rowsToRetry) {
    QJsonObject error = m_errorsModel->getError(row);
    QJsonObject request = error.value(QStringLiteral("request")).toObject();

    // Create a QueueItem and embed the entire error object as context
    // This maintains the error history which was previously tracked implicitly
    // or manually.
    QueueItem item;
    item.requestData = request;
    // Track error history natively through the requestData wrapper if
    // supported, or just re-add to queue and letting API retry.
    if (error.contains(QStringLiteral("pastErrors"))) {
      item.pastErrors = error.value(QStringLiteral("pastErrors")).toArray();
    }
    QJsonObject strippedError = error;
    strippedError.remove(QStringLiteral("pastErrors"));
    item.pastErrors.append(strippedError); // append current error

    m_queueModel->enqueueItem(item);
    m_errorsModel->removeError(row);
  }

  if (!rowsToRetry.isEmpty() && !m_queueTimer->isActive() && !m_queuePaused) {
    m_queueTimer->start(1000);
  }
}

void MainWindow::processQueue() {
  if (m_queueModel->isEmpty()) {
    if (m_queueTimer->isActive()) {
      KNotification *notification = new KNotification(
          QStringLiteral("queueEmpty"), KNotification::CloseOnTimeout, this);
      notification->setTitle(i18n("Queue Empty"));
      notification->setText(i18n("The processing queue is now empty."));
      connect(notification, &KNotification::closed, notification,
              &QObject::deleteLater);
      notification->sendEvent();
      m_queueTimer->stop();
    }
    return;
  }

  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));

  QString currentQueueMode = queueConfig.readEntry("QueueMode", QString());
  if (currentQueueMode.isEmpty()) {
    currentQueueMode = queueConfig.readEntry("OneAtATimeMode", false)
                           ? QStringLiteral("one_at_a_time")
                           : QStringLiteral("asap");
  }
  bool oneAtATimeMode = currentQueueMode == QStringLiteral("one_at_a_time");

  int globalOneAtATimeLimit = queueConfig.readEntry("OneAtATimeLimit", 1);
  KConfigGroup sourceConcurrencyConfig(KSharedConfig::openConfig(),
                                       QStringLiteral("SourceConcurrency"));

  int processIndex = -1;
  QHash<QString, int> activeCountCache;

  // Pre-calculate active session counts from m_sessionModel
  for (int j = 0; j < m_sessionModel->rowCount(); ++j) {
    QModelIndex idx = m_sessionModel->index(j, 0);
    QString state =
        m_sessionModel->data(idx, SessionModel::StateRole).toString();
    if (state == QStringLiteral("PENDING") ||
        state == QStringLiteral("IN_PROGRESS")) {
      QString source =
          m_sessionModel->data(idx, SessionModel::SourceRole).toString();
      activeCountCache[source]++;
    }
  }

  m_queueModel->beginBatchUpdate();

  for (int i = 0; i < m_queueModel->rowCount(); ++i) {
    QueueItem item = m_queueModel->getItem(i);
    bool needsUpdate = false;

    if (item.isWaitItem) {
      if (processIndex == -1) {
        processIndex =
            i; // We hit a wait item before any valid processable item
      }
      continue;
    }

    QString source = item.requestData.value(QStringLiteral("sourceContext"))
                         .toObject()
                         .value(QStringLiteral("source"))
                         .toString();
    if (source.isEmpty()) {
      source = item.requestData.value(QStringLiteral("source")).toString();
    }

    int sourceLimit = sourceConcurrencyConfig.readEntry(source, -1);
    bool checkLimit = (sourceLimit != 0) && (oneAtATimeMode || sourceLimit > 0);
    int effectiveLimit =
        (sourceLimit > 0) ? sourceLimit : globalOneAtATimeLimit;

    if (item.blockMetadata.value(QStringLiteral("forced")).toBool()) {
      checkLimit = false;
    }

    if (!checkLimit) {
      if (item.isBlocked) {
        item.isBlocked = false;
        item.blockMetadata = QJsonObject();
        needsUpdate = true;
      }
      if (processIndex == -1) {
        processIndex = i;
      }
      activeCountCache[source]++;
      if (needsUpdate)
        m_queueModel->updateItem(i, item);
      continue;
    }

    if (activeCountCache[source] >= effectiveLimit) {
      if (!item.isBlocked) {
        item.isBlocked = true;
        QJsonObject meta;
        meta[QStringLiteral("reason")] =
            QStringLiteral("Concurrency limit reached");
        meta[QStringLiteral("source")] = source;
        item.blockMetadata = meta;
        needsUpdate = true;
      }
    } else {
      if (item.isBlocked) {
        item.isBlocked = false;
        item.blockMetadata = QJsonObject();
        needsUpdate = true;
      }
      if (processIndex == -1) {
        processIndex = i;
      }
      activeCountCache[source]++; // We consider this one active now
    }

    if (needsUpdate) {
      m_queueModel->updateItem(i, item);
    }
  }

  m_queueModel->endBatchUpdate();

  if (m_isProcessingQueue || m_queuePaused)
    return;

  if (m_queueBackoffUntil.isValid() &&
      QDateTime::currentDateTimeUtc() < m_queueBackoffUntil) {
    // We are backing off
    return;
  }

  if (processIndex == -1) {
    // Everything is blocked or queue is essentially empty
    return;
  }

  if (processIndex != 0) {
    m_queueModel->moveItem(processIndex, 0);
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

  QueueItem item = m_queueModel->peek(); // Peek the item we were processing
  m_queueModel
      ->recordRun(); // Record that we completed a run successfully or not
  m_isProcessingQueue = false;

  if (success) {
    m_queueModel->dequeue(); // Pop the item only on successful state transition
    m_queueModel
        ->checkAndPrependDailyLimitWait(); // Dynamically check after a run
    m_sessionModel->addSession(session);
    QString sourceId = session.value(QStringLiteral("sourceContext"))
                           .toObject()
                           .value(QStringLiteral("source"))
                           .toString();
    if (!sourceId.isEmpty())
      m_sourceModel->recordSessionCreated(sourceId);
    updateStatus(i18n("Session created from queue."));
    ActivityLogWindow::instance()->logMessage(
        i18n("Processed schedule run: Session created for %1.", sourceId));
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

      // The item remains at the front of the queue.

      // Apply a short backoff (e.g. 5 minutes)
      QueueItem waitItem;
      waitItem.isWaitItem = true;
      KConfigGroup queueConfig(KSharedConfig::openConfig(),
                               QStringLiteral("Queue"));
      int backoffMins = queueConfig.readEntry("PreconditionBackoffInterval", 5);
      waitItem.waitSeconds = qMin(static_cast<qint64>(backoffMins) * 60,
                                  QueueModel::maxBackoffSeconds());
      m_queueModel->prependWaitItem(waitItem);
      m_queueBackoffUntil = QDateTime(); // Clear backoff
    } else if (isResourceExhausted) {
      updateStatus(i18n("API Rate limit hit, adding a wait item..."));

      // The item remains at the front of the queue.

      // Prepend a wait item
      QueueItem waitItem;
      waitItem.isWaitItem = true;
      waitItem.waitSeconds = qMin(3600LL, QueueModel::maxBackoffSeconds());
      m_queueModel->prependWaitItem(waitItem);
      m_queueBackoffUntil = QDateTime(); // Clear backoff
    } else {
      updateStatus(i18n("Failed to create session from queue: %1", errorMsg));
      m_queueModel->dequeue(); // Pop the item out of the queue because it is in
                               // Error state
      // The error is recorded to the error model through the APIManager error
      // signal handlers and onSessionCreationFailed.
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
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  window->setInitialData(templateData);

  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(window, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  connect(window, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);

  window->show();
}

void MainWindow::onQueueActivated(const QModelIndex &index) {
  if (!index.isValid())
    return;
  int row = index.row();
  QueueItem item = m_queueModel->getItem(row);
  if (item.errorCount > 0) {
    showErrorDetails(row, m_queueModel);
  } else {
    editQueueItem(row);
  }
}

void MainWindow::updateHoldingTabVisibility() {
  int holdingIdx = m_tabWidget->indexOf(m_holdingView);
  if (m_holdingModel->isEmpty()) {
    if (holdingIdx != -1) {
      m_tabWidget->removeTab(holdingIdx);
    }
  } else {
    if (holdingIdx == -1) {
      int queueIdx = m_tabWidget->indexOf(m_queueView);
      if (queueIdx != -1) {
        holdingIdx = m_tabWidget->insertTab(queueIdx + 1, m_holdingView,
                                            i18n("Holding"));
      } else {
        holdingIdx = m_tabWidget->addTab(m_holdingView, i18n("Holding"));
      }
    }
    m_tabWidget->setTabText(holdingIdx,
                            i18n("Holding (%1)", m_holdingModel->size()));
  }
}

void MainWindow::updateBlockedTabVisibility() {
  int blockedIdx = m_tabWidget->indexOf(m_blockedView);
  int blockedItems = m_blockedTreeModel->totalBlockedItemsCount();
  int blockedSources = m_blockedTreeModel->blockedSourcesCount();

  if (blockedItems == 0) {
    if (blockedIdx != -1) {
      m_tabWidget->removeTab(blockedIdx);
    }
  } else {
    if (blockedIdx == -1) {
      int holdingIdx = m_tabWidget->indexOf(m_holdingView);
      if (holdingIdx != -1) {
        blockedIdx = m_tabWidget->insertTab(holdingIdx + 1, m_blockedView,
                                            i18n("Blocked"));
      } else {
        int queueIdx = m_tabWidget->indexOf(m_queueView);
        if (queueIdx != -1) {
          blockedIdx = m_tabWidget->insertTab(queueIdx + 1, m_blockedView,
                                              i18n("Blocked"));
        } else {
          blockedIdx = m_tabWidget->addTab(m_blockedView, i18n("Blocked"));
        }
      }
    }
    m_tabWidget->setTabText(blockedIdx, i18n("Blocked (%1)", blockedSources));
  }
}

void MainWindow::onHoldingActivated(const QModelIndex &index) {
  int row = index.row();
  QueueItem item = m_holdingModel->getItem(row);
  if (item.errorCount > 0) {
    showErrorDetails(row, m_holdingModel);
  }
}

void MainWindow::onBlockedContextMenu(const QPoint &pos) {
  QModelIndex index = m_blockedView->indexAt(pos);
  if (!index.isValid())
    return;

  bool isSource =
      m_blockedTreeModel->data(index, BlockedTreeModel::IsSourceRole).toBool();

  int queueIndex =
      m_blockedTreeModel->data(index, BlockedTreeModel::QueueIndexRole).toInt();

  QMenu menu(this);
  if (isSource) {
    QString id = m_blockedTreeModel->data(index, BlockedTreeModel::SourceIdRole)
                     .toString();
    QAction *configLimitAction =
        menu.addAction(i18n("Configure Concurrency Limit"));
    connect(configLimitAction, &QAction::triggered, [this, id]() {
      KConfigGroup sourceConfig(KSharedConfig::openConfig(),
                                QStringLiteral("SourceConcurrency"));
      int currentLimit = sourceConfig.readEntry(id, -1);

      bool ok;
      int newLimit =
          QInputDialog::getInt(this, i18n("Concurrency Limit"),
                               i18n("Enter max active sessions for %1 (0 to "
                                    "disable limit, -1 to defer to global):",
                                    id),
                               currentLimit, -1, 1000, 1, &ok);
      if (ok) {
        if (newLimit == -1) {
          sourceConfig.deleteEntry(id);
        } else {
          sourceConfig.writeEntry(id, newLimit);
        }
        sourceConfig.sync();
        updateStatus(
            i18n("Concurrency limit for %1 updated to %2", id, newLimit));
        QTimer::singleShot(0, this, &MainWindow::processQueue);
      }
    });
  } else {
    QAction *forceAction = menu.addAction(i18n("Force into queue (unblock)"));
    connect(forceAction, &QAction::triggered, this, [this, queueIndex]() {
      QueueItem item = m_queueModel->getItem(queueIndex);
      item.isBlocked = false;
      QJsonObject meta;
      meta[QStringLiteral("forced")] = true;
      item.blockMetadata = meta;
      m_queueModel->updateItem(queueIndex, item);
      m_queueModel->moveItem(queueIndex, 0);
      QTimer::singleShot(0, this, &MainWindow::processQueue);
    });
  }
  menu.exec(m_blockedView->viewport()->mapToGlobal(pos));
}

void MainWindow::onHoldingContextMenu(const QPoint &pos) {
  QModelIndex index = m_holdingView->indexAt(pos);
  if (!index.isValid())
    return;
  if (!m_holdingView->selectionModel()->isSelected(index)) {
    m_holdingView->selectionModel()->select(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_holdingView->setCurrentIndex(index);
  }
  int row = index.row();

  QMenu menu;
  QAction *moveUpAction = nullptr;
  QAction *moveDownAction = nullptr;

  if (m_holdingView->selectionModel()->selectedRows().size() == 1) {
    if (row > 0) {
      moveUpAction = menu.addAction(QIcon::fromTheme(QStringLiteral("go-up")),
                                    i18n("Move Up"));
    }
    if (row < m_holdingModel->rowCount() - 1) {
      moveDownAction = menu.addAction(
          QIcon::fromTheme(QStringLiteral("go-down")), i18n("Move Down"));
    }
    if (moveUpAction || moveDownAction) {
      menu.addSeparator();
    }
  }

  QAction *requeueAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("go-up")), i18n("Requeue"));
  QAction *deleteAction = menu.addAction(
      QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));

  QAction *selected = menu.exec(m_holdingView->viewport()->mapToGlobal(pos));
  if (moveUpAction && selected == moveUpAction) {
    m_holdingModel->moveItem(row, row - 1);
  } else if (moveDownAction && selected == moveDownAction) {
    m_holdingModel->moveItem(row, row + 1);
  } else if (selected == requeueAction) {
    QModelIndexList selectedRows =
        m_holdingView->selectionModel()->selectedRows();
    QList<int> rowsToRequeue;
    for (const QModelIndex &idx : selectedRows) {
      if (!rowsToRequeue.contains(idx.row())) {
        rowsToRequeue.append(idx.row());
      }
    }
    std::sort(rowsToRequeue.begin(), rowsToRequeue.end(), std::greater<int>());
    for (int r : rowsToRequeue) {
      QueueItem item = m_holdingModel->getItem(r);
      m_holdingModel->removeItem(r);
      item.isWaitItem = false;
      m_queueModel->enqueueItem(item);
    }
    updateStatus(i18np("Task moved back to queue.",
                       "%1 tasks moved back to queue.", rowsToRequeue.size()));
  } else if (selected == deleteAction) {
    if (m_deleteHoldingItemsLambda) {
      m_deleteHoldingItemsLambda();
    }
  }
}

void MainWindow::onQueueContextMenu(const QPoint &pos) {
  QModelIndex index = m_queueView->indexAt(pos);
  if (!index.isValid())
    return;
  if (!m_queueView->selectionModel()->isSelected(index)) {
    m_queueView->selectionModel()->select(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_queueView->setCurrentIndex(index);
  }
  int row = index.row();

  QueueItem item = m_queueModel->getItem(row);

  QMenu menu;

  QAction *moveUpAction = nullptr;
  QAction *moveDownAction = nullptr;

  if (m_queueView->selectionModel()->selectedRows().size() == 1) {
    if (row > 0) {
      moveUpAction = menu.addAction(QIcon::fromTheme(QStringLiteral("go-up")),
                                    i18n("Move Up"));
    }
    if (row < m_queueModel->rowCount() - 1) {
      moveDownAction = menu.addAction(
          QIcon::fromTheme(QStringLiteral("go-down")), i18n("Move Down"));
    }
    if (moveUpAction || moveDownAction) {
      menu.addSeparator();
    }
  }

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
  menu.addSeparator();
  QAction *holdAction =
      menu.addAction(QIcon::fromTheme(QStringLiteral("media-playback-pause")),
                     i18n("Move to Holding"));

  QAction *selected = menu.exec(m_queueView->viewport()->mapToGlobal(pos));
  if (moveUpAction && selected == moveUpAction) {
    m_queueModel->moveItem(row, row - 1);
  } else if (moveDownAction && selected == moveDownAction) {
    m_queueModel->moveItem(row, row + 1);
  } else if (errorAction && selected == errorAction) {
    showErrorDetails(row, m_queueModel);
  } else if (selected == editAction) {
    editQueueItem(row);
  } else if (selected == deleteAction) {
    if (m_deleteQueueItemsLambda) {
      m_deleteQueueItemsLambda();
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
  } else if (selected == holdAction) {
    QModelIndexList selectedRows =
        m_queueView->selectionModel()->selectedRows();
    QList<int> rowsToHold;
    for (const QModelIndex &idx : selectedRows) {
      if (!rowsToHold.contains(idx.row())) {
        QueueItem checkItem = m_queueModel->getItem(idx.row());
        if (!checkItem.isWaitItem) {
          rowsToHold.append(idx.row());
        }
      }
    }
    std::sort(rowsToHold.begin(), rowsToHold.end(), std::greater<int>());
    for (int r : rowsToHold) {
      QueueItem holdItem = m_queueModel->getItem(r);
      m_queueModel->removeItem(r);
      holdItem.isWaitItem = false;
      m_holdingModel->enqueueItem(holdItem);
    }
    if (rowsToHold.size() > 0) {
      updateStatus(i18np("Task moved to holding queue.",
                         "%1 tasks moved to holding queue.",
                         rowsToHold.size()));
    } else {
      updateStatus(
          i18n("No tangible tasks selected to move to holding queue."));
    }
  }
}

void MainWindow::sendQueueItemNow(int row) {
  QueueItem item = m_queueModel->getItem(row);
  if (item.isWaitItem) {
    m_queueModel->removeItem(row);
    updateStatus(i18n("Wait item removed from queue."));
    if (row == 0) {
      QTimer::singleShot(0, this, &MainWindow::processQueue);
    }
    return;
  }
  if (item.requestData.isEmpty())
    return;
  m_queueModel->removeItem(row);
  updateStatus(i18n("Sending queue item immediately..."));
  m_apiManager->createSessionAsync(item.requestData);
}

void MainWindow::requeueError(int sourceRow) {
  QJsonObject errData = m_errorsModel->getError(sourceRow);
  QJsonObject req = errData.value(QStringLiteral("request")).toObject();
  QueueItem item;
  item.requestData = req;
  if (errData.contains(QStringLiteral("pastErrors"))) {
    item.pastErrors = errData.value(QStringLiteral("pastErrors")).toArray();
  }
  QJsonObject strippedError = errData;
  strippedError.remove(QStringLiteral("pastErrors"));
  item.pastErrors.append(strippedError);
  m_queueModel->enqueueItem(item);
  m_errorsModel->removeError(sourceRow);
}

void MainWindow::showErrorDetails(int row, QueueModel *model) {
  QueueItem item = model->getItem(row);
  if (item.requestData.isEmpty())
    return;

  ErrorWindow *window = new ErrorWindow(row, item, this);
  connect(window, &ErrorWindow::editRequested, this,
          &MainWindow::editQueueItem);
  connect(window, &ErrorWindow::deleteRequested, this, [this, model](int r) {
    model->removeItem(r);
    updateStatus(i18n("Task removed."));
  });
  connect(window, &ErrorWindow::draftRequested, this,
          &MainWindow::convertQueueItemToDraft);
  connect(window, &ErrorWindow::templateRequested, this, [this, row, model]() {
    QueueItem item = model->getItem(row);
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
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  window->setEditMode(true);
  window->setInitialData(item.requestData);

  QPersistentModelIndex persistentIndex(m_queueModel->index(row, 0));

  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested,
          [this, persistentIndex](const QMultiMap<QString, QString> &sources,
                                  const QString &p, const QString &a,
                                  bool requirePlanApproval) {
            if (persistentIndex.isValid()) {
              m_queueModel->removeItem(persistentIndex.row());
            }
            onSessionCreated(sources, p, a, requirePlanApproval);
          });

  connect(window, &NewSessionDialog::saveDraftRequested,
          [this, persistentIndex](const QJsonObject &d) {
            if (persistentIndex.isValid()) {
              m_queueModel->removeItem(persistentIndex.row());
            }
            m_draftsModel->addDraft(d);
            updateStatus(i18n("Task moved to drafts."));
          });

  window->show();
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
  bool isRateLimit = false;
  if (m_isProcessingQueue) {
    QJsonObject errObj = response.value(QStringLiteral("error")).toObject();
    QString status = errObj.value(QStringLiteral("status")).toString();
    int code = errObj.value(QStringLiteral("code")).toInt();
    if (status == QStringLiteral("FAILED_PRECONDITION") ||
        status == QStringLiteral("RESOURCE_EXHAUSTED") || code == 429) {
      isRateLimit = true;
    }
  }

  if (!isRateLimit) {
    QJsonObject errorObj;
    errorObj[QStringLiteral("request")] = request;
    errorObj[QStringLiteral("response")] = response;
    errorObj[QStringLiteral("message")] = errorString;
    if (!httpDetails.isEmpty()) {
      errorObj[QStringLiteral("httpDetails")] = httpDetails;
    }
    errorObj[QStringLiteral("timestamp")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // If it was a queued item, copy over past errors if they exist.
    // However, onSessionCreationFailed only takes request, not the QueueItem
    // directly. As a simplification, we can look up the peeked queue item to
    // extract its pastErrors.
    if (m_isProcessingQueue && m_queueModel->size() > 0) {
      QueueItem item = m_queueModel->peek();
      if (item.requestData == request && !item.pastErrors.isEmpty()) {
        errorObj[QStringLiteral("pastErrors")] = item.pastErrors;
      }
    }

    m_errorsModel->addErrorObj(errorObj);
    updateStatus(i18n("Error saved."));

    // We capture request object to uniquely identify the error instead of
    // relying on shifting index
    QJsonObject requestCopy = request;

    KNotification *notification = new KNotification(
        QStringLiteral("queueError"), KNotification::CloseOnTimeout, this);
    notification->setTitle(i18n("Queue Error"));
    notification->setText(i18n("A task encountered an error: %1", errorString));

    connect(notification, &KNotification::closed, notification,
            &QObject::deleteLater);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    notification->setDefaultAction(i18n("View"));
    connect(notification, &KNotification::defaultActivated, this,
            [this, requestCopy]() {
#else
    auto action = notification->addDefaultAction(i18n("View"));
    connect(action, &KNotificationAction::activated, this,
            [this, requestCopy]() {
#endif
              if (m_tabWidget) {
                for (int i = 0; i < m_tabWidget->count(); ++i) {
                  if (m_tabWidget->widget(i) == m_errorsView->parentWidget()) {
                    m_tabWidget->setCurrentIndex(i);
                    break;
                  }
                }
              }

              // Find the index by request data as rows can shift
              int targetRow = -1;
              for (int i = 0; i < m_errorsModel->rowCount(); ++i) {
                QJsonObject err = m_errorsModel->getError(i);
                if (err.value(QStringLiteral("request")).toObject() ==
                    requestCopy) {
                  targetRow = i;
                  break;
                }
              }

              if (targetRow != -1) {
                QModelIndex idx = m_errorsModel->index(targetRow, 0);
                if (idx.isValid()) {
                  onErrorActivated(idx);
                }
              }
            });

    notification->sendEvent();
  }

  if (isRateLimit) {
    // These are rate-limiting/concurrency errors handled by the queue's wait
    // mechanism. Do not pop up an error modal for them.
    return;
  }
}

void MainWindow::onErrorActivated(const QModelIndex &index) {
  QJsonObject errorData = m_errorsModel->getError(index.row());
  QJsonObject request = errorData.value(QStringLiteral("request")).toObject();

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  window->setInitialData(request);

  QPersistentModelIndex persistentIndex(index);

  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested,
          [this, persistentIndex](const QMultiMap<QString, QString> &sources,
                                  const QString &p, const QString &a,
                                  bool requirePlanApproval) {
            onSessionCreated(sources, p, a, requirePlanApproval);
            if (persistentIndex.isValid()) {
              m_errorsModel->removeError(persistentIndex.row());
            }
          });

  connect(window, &NewSessionDialog::saveDraftRequested,
          [this, persistentIndex](const QJsonObject &d) {
            m_draftsModel->addDraft(d);
            if (persistentIndex.isValid()) {
              m_errorsModel->removeError(persistentIndex.row());
            }
            updateStatus(i18n("Draft saved and error removed."));
          });

  window->show();
}

void MainWindow::onDraftActivated(const QModelIndex &index) {
  QJsonObject draft = m_draftsModel->getDraft(index.row());
  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  window->setInitialData(draft);

  QPersistentModelIndex persistentIndex(index);

  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested,
          [this, persistentIndex](const QMultiMap<QString, QString> &sources,
                                  const QString &p, const QString &a,
                                  bool requirePlanApproval) {
            onSessionCreated(sources, p, a, requirePlanApproval);
            if (persistentIndex.isValid()) {
              m_draftsModel->removeDraft(persistentIndex.row());
            }
          });

  connect(window, &NewSessionDialog::saveDraftRequested,
          [this, persistentIndex](const QJsonObject &d) {
            if (persistentIndex.isValid()) {
              m_draftsModel->removeDraft(persistentIndex.row());
            }
            m_draftsModel->addDraft(d);
            updateStatus(i18n("Draft updated."));
          });
  connect(window, &NewSessionDialog::saveTemplateRequested, this,
          &MainWindow::onTemplateSaved);

  window->show();
}

void MainWindow::onSourceActivated(const QModelIndex &index) {
  // Map index from proxy to source
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

  QJsonObject initData;
  QJsonArray sourcesArr;

  QModelIndexList selection = m_sourceView->selectionModel()->selectedRows();
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
  auto window =
      new NewSessionDialog(m_sourceModel, m_templatesModel, hasApiKey, this);
  window->setInitialData(initData);

  connectNewSessionDialog(window);
  connect(window, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(window, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  window->show();
}

void MainWindow::connectNewSessionDialog(NewSessionDialog *window) {
  connect(window, &NewSessionDialog::refreshSourcesRequested, this,
          &MainWindow::refreshSources);
  connect(this, &MainWindow::statusMessage, window,
          &NewSessionDialog::updateStatus);
  connect(window, &NewSessionDialog::refreshGithubRequested, this,
          &MainWindow::refreshGithubDataForSources);
  connect(window, &NewSessionDialog::refreshSourceRequested, this,
          [this](const QString &id) { m_apiManager->getSource(id); });
}

void MainWindow::connectSessionWindow(SessionWindow *window) {
  connect(window, &SessionWindow::duplicateRequested, this,
          [this](const QJsonObject &sessionData) {
            QJsonObject initData;
            initData[QStringLiteral("prompt")] =
                sessionData.value(QStringLiteral("prompt")).toString();
            const QJsonObject sourceContext =
                sessionData.value(QStringLiteral("sourceContext")).toObject();
            const QString source =
                sourceContext.value(QStringLiteral("source")).toString();
            if (!source.isEmpty()) {
              QJsonObject sourceObj;
              sourceObj[QStringLiteral("name")] = source;
              const QString branch =
                  sourceContext.value(QStringLiteral("githubRepoContext"))
                      .toObject()
                      .value(QStringLiteral("startingBranch"))
                      .toString();
              if (!branch.isEmpty()) {
                sourceObj[QStringLiteral("branch")] = branch;
              }
              initData[QStringLiteral("sources")] = QJsonArray{sourceObj};
            }
            showNewSessionDialog(initData);
          });

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
  m_sessionModel->markAsRead(sessionId);
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

  QString id =
      m_sessionModel->data(sourceIndex, SessionModel::IdRole).toString();
  m_sessionModel->clearUnreadChanges(id);

  if (sessionData.isEmpty()) {
    m_sessionModel->markAsRead(id);
    m_apiManager->getSession(id);
    updateStatus(i18n("Fetching details for session %1...", id));
  } else {
    QString sessionId = sessionData.value(QStringLiteral("id")).toString();
    m_sessionModel->markAsRead(sessionId);
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
  Q_EMIT statusMessage(message);
}

void MainWindow::refreshGithubDataForSources(const QStringList &sourceIds) {
  updateStatus(i18n("Refreshing GitHub data for selected sources..."));
  if (m_apiManager->githubToken().isEmpty()) {
    updateStatus(i18n("Cannot refresh GitHub data: No GitHub token."));
    return;
  }
  for (const QString &id : sourceIds) {
    if (id.startsWith(QStringLiteral("sources/github/"))) {
      m_apiManager->fetchGithubInfo(id);
      m_apiManager->fetchGithubBranches(id);
    }
  }
  updateStatus(
      i18n("GitHub data refresh requested for %1 sources.", sourceIds.size()));
}

void MainWindow::onError(const QString &message) {
  updateStatus(i18n("Error: %1", message));
  QMessageBox::critical(this, i18n("Error"), message);
}

void MainWindow::toggleWindow() { toggleWindowVisibility(); }

void MainWindow::toggleFavourite() {
  if (m_tabWidget->currentWidget()->objectName() ==
      QStringLiteral("sourcesTab")) {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
    if (proxy) {
      for (const QModelIndex &idx : selectedRows) {
        QModelIndex sourceIndex = proxy->mapToSource(idx);
        QString id =
            m_sourceModel->data(sourceIndex, SourceModel::IdRole).toString();
        m_sourceModel->toggleFavourite(id);
      }
    }
  } else if (m_tabWidget->currentWidget()->objectName() ==
             QStringLiteral("followingTab")) {
    QModelIndexList selectedRows =
        m_sessionView->selectionModel()->selectedRows();
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
    if (proxy) {
      for (const QModelIndex &idx : selectedRows) {
        QModelIndex sourceIndex = proxy->mapToSource(idx);
        QString id =
            m_sessionModel->data(sourceIndex, SessionModel::IdRole).toString();
        m_sessionModel->toggleFavourite(id);
      }
    }
  } else if (m_tabWidget->currentWidget()->objectName() ==
             QStringLiteral("archiveTab")) {
    QModelIndexList selectedRows =
        m_archiveView->selectionModel()->selectedRows();
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_archiveView->model());
    if (proxy) {
      for (const QModelIndex &idx : selectedRows) {
        QModelIndex sourceIndex = proxy->mapToSource(idx);
        QString id =
            m_archiveModel->data(sourceIndex, SessionModel::IdRole).toString();
        m_archiveModel->toggleFavourite(id);
      }
    }
  }
}

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

  m_sourceProgressBar->setFormat(i18n("%1 sources loaded from %2 pages",
                                      m_sourcesLoadedCount,
                                      m_pagesLoadedCount));
  updateStatus(i18n("Loaded %1 sources, added %2 new.", m_sourcesLoadedCount,
                    m_sourcesAddedCount));
}

void MainWindow::onGithubBranchesReceived(const QString &sourceId,
                                          const QJsonArray &branches) {
  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex index = m_sourceModel->index(i, 0);
    QString id = m_sourceModel->data(index, SourceModel::IdRole).toString();
    if (id == sourceId) {
      QJsonObject source =
          m_sourceModel->data(index, SourceModel::RawDataRole).toJsonObject();
      QJsonObject github = source.value(QStringLiteral("github")).toObject();

      QJsonArray branchNames;
      for (const QJsonValue &v : branches) {
        QJsonObject branchObj = v.toObject();
        if (branchObj.contains(QStringLiteral("name"))) {
          branchNames.append(
              branchObj.value(QStringLiteral("name")).toString());
        }
      }

      github[QStringLiteral("branches")] = branchNames;
      source[QStringLiteral("github")] = github;
      m_sourceModel->updateSource(source);
      break;
    }
  }
}

void MainWindow::onGithubInfoReceived(const QString &sourceId,
                                      const QJsonObject &info) {
  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex index = m_sourceModel->index(i, 0);
    QString id = m_sourceModel->data(index, SourceModel::IdRole).toString();
    if (id == sourceId) {
      QJsonObject source =
          m_sourceModel->data(index, SourceModel::RawDataRole).toJsonObject();

      source[QStringLiteral("description")] =
          info.value(QStringLiteral("description")).toString();
      source[QStringLiteral("isArchived")] =
          info.value(QStringLiteral("archived")).toBool();
      source[QStringLiteral("isFork")] =
          info.value(QStringLiteral("fork")).toBool();
      source[QStringLiteral("isPrivate")] =
          info.value(QStringLiteral("private")).toBool();
      source[QStringLiteral("language")] =
          info.value(QStringLiteral("language")).toString();
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
    if (m_sourcesAddedCount > 0) {
      ActivityLogWindow::instance()->logMessage(
          i18np("Source refresh completed: 1 new source found.",
                "Source refresh completed: %1 new sources found.",
                m_sourcesAddedCount));

      KNotification *notification =
          new KNotification(QStringLiteral("sourcesRefreshFinished"),
                            KNotification::CloseOnTimeout, this);
      notification->setTitle(i18n("Sources Refresh Finished"));
      notification->setText(
          i18np("Loaded %2 sources in total, 1 new source found.",
                "Loaded %2 sources in total, %1 new sources found.",
                m_sourcesAddedCount, m_sourcesLoadedCount));
      connect(notification, &KNotification::closed, notification,
              &QObject::deleteLater);
      notification->sendEvent();
    }
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

void MainWindow::autoRefreshFollowing() {
  for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
    QModelIndex index = m_sessionModel->index(i, 0);
    QString state =
        m_sessionModel->data(index, SessionModel::StateRole).toString();
    if (state == QStringLiteral("DONE") ||
        state == QStringLiteral("CANCELED") ||
        state == QStringLiteral("ERROR")) {
      continue;
    }
    QString currentId =
        m_sessionModel->data(index, SessionModel::IdRole).toString();
    if (!currentId.isEmpty()) {
      m_apiManager->reloadSession(currentId);
    }
  }
  m_lastSessionRefreshTime = QDateTime::currentDateTime();
  updateSessionStats();
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
  m_followingFilterEditor->setCompletions(completions);
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

void MainWindow::updateFavouritesMenu() {
  if (!m_favouritesMenu)
    return;

  m_favouritesMenu->clear();

  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex idx = m_sourceModel->index(i, 0);
    if (m_sourceModel->data(idx, SourceModel::FavouriteRole).toBool()) {
      QString name = m_sourceModel->data(idx, Qt::DisplayRole).toString();
      QAction *action = m_favouritesMenu->addAction(
          QIcon::fromTheme(QStringLiteral("folder-bookmark")), name);
      connect(action, &QAction::triggered, this,
              [this, persistentIdx = QPersistentModelIndex(idx)]() {
                if (persistentIdx.isValid()) {
                  QJsonObject initData;
                  QJsonArray sourcesArr;
                  QString srcName =
                      m_sourceModel->data(persistentIdx, SourceModel::NameRole)
                          .toString();
                  sourcesArr.append(srcName);
                  initData[QStringLiteral("sources")] = sourcesArr;
                  showNewSessionDialog(initData);
                }
              });
    }
  }

  int sessionCount = 0;
  auto processSessionModel = [this, &sessionCount](SessionModel *model) {
    for (int i = 0; i < model->rowCount(); ++i) {
      QModelIndex idx = model->index(i, 0);
      if (model->data(idx, SessionModel::FavouriteRole).toBool()) {
        if (sessionCount == 0 && !m_favouritesMenu->actions().isEmpty()) {
          m_favouritesMenu->addSeparator();
        }
        QString id = model->data(idx, SessionModel::IdRole).toString();
        QString name = model->getSessionName(id);
        if (name.isEmpty())
          name = id;
        QAction *action = m_favouritesMenu->addAction(
            QIcon::fromTheme(QStringLiteral("emblem-favorite")), name);
        connect(action, &QAction::triggered, this, [this, id]() {
          QJsonObject sessionData;
          bool isManaged = false;
          for (int j = 0; j < m_sessionModel->rowCount(); ++j) {
            if (m_sessionModel
                    ->data(m_sessionModel->index(j, 0), SessionModel::IdRole)
                    .toString() == id) {
              sessionData = m_sessionModel->getSession(j);
              isManaged = true;
              break;
            }
          }
          if (sessionData.isEmpty()) {
            for (int j = 0; j < m_archiveModel->rowCount(); ++j) {
              if (m_archiveModel
                      ->data(m_archiveModel->index(j, 0), SessionModel::IdRole)
                      .toString() == id) {
                sessionData = m_archiveModel->getSession(j);
                isManaged = true;
                break;
              }
            }
          }
          if (!sessionData.isEmpty()) {
            SessionWindow *window =
                new SessionWindow(sessionData, m_apiManager, isManaged, this);
            connectSessionWindow(window);
            window->show();
          }
        });
        sessionCount++;
      }
    }
  };

  processSessionModel(m_sessionModel);
  processSessionModel(m_archiveModel);

  if (m_favouritesMenu->actions().isEmpty()) {
    QAction *emptyAction = m_favouritesMenu->addAction(i18n("No Favourites"));
    emptyAction->setEnabled(false);
  }
}

void MainWindow::applyFavouriteAction(
    std::function<void(const QSortFilterProxyModel *, QAbstractItemModel *,
                       const QModelIndexList &, int)>
        action) {
  QAbstractItemView *view = nullptr;
  QAbstractItemModel *model = nullptr;
  int idRole = -1;

  if (m_tabWidget->currentWidget()->objectName() ==
      QStringLiteral("sourcesTab")) {
    view = m_sourceView;
    model = m_sourceModel;
    idRole = SourceModel::IdRole;
  } else if (m_tabWidget->currentWidget()->objectName() ==
             QStringLiteral("followingTab")) {
    view = m_sessionView;
    model = m_sessionModel;
    idRole = SessionModel::IdRole;
  } else if (m_tabWidget->currentWidget()->objectName() ==
             QStringLiteral("archiveTab")) {
    view = m_archiveView;
    model = m_archiveModel;
    idRole = SessionModel::IdRole;
  }

  if (!view || !model)
    return;

  QModelIndexList selectedRows = view->selectionModel()->selectedRows();
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(view->model());

  if (proxy && !selectedRows.isEmpty()) {
    action(proxy, model, selectedRows, idRole);
  }
}

void MainWindow::increaseFavouriteRank() {
  applyFavouriteAction([](const QSortFilterProxyModel *proxy,
                          QAbstractItemModel *model,
                          const QModelIndexList &selectedRows, int idRole) {
    SourceModel *sm = qobject_cast<SourceModel *>(model);
    SessionModel *sessionModel = qobject_cast<SessionModel *>(model);
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex sourceIndex = proxy->mapToSource(idx);
      QString id = model->data(sourceIndex, idRole).toString();
      if (sm) {
        sm->increaseFavouriteRank(id);
      } else if (sessionModel) {
        sessionModel->increaseFavouriteRank(id);
      }
    }
  });
}

void MainWindow::decreaseFavouriteRank() {
  applyFavouriteAction([](const QSortFilterProxyModel *proxy,
                          QAbstractItemModel *model,
                          const QModelIndexList &selectedRows, int idRole) {
    SourceModel *sm = qobject_cast<SourceModel *>(model);
    SessionModel *sessionModel = qobject_cast<SessionModel *>(model);
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex sourceIndex = proxy->mapToSource(idx);
      QString id = model->data(sourceIndex, idRole).toString();
      if (sm) {
        sm->decreaseFavouriteRank(id);
      } else if (sessionModel) {
        sessionModel->decreaseFavouriteRank(id);
      }
    }
  });
}

void MainWindow::setFavouriteRank() {
  applyFavouriteAction([this](const QSortFilterProxyModel *proxy,
                              QAbstractItemModel *model,
                              const QModelIndexList &selectedRows, int idRole) {
    int initialRank = 1;
    QModelIndex sourceIndex = proxy->mapToSource(selectedRows.first());

    int favRole = (idRole == SourceModel::IdRole)
                      ? static_cast<int>(SourceModel::FavouriteRole)
                      : static_cast<int>(SessionModel::FavouriteRole);
    QVariant rankVal = model->data(sourceIndex, favRole);
    if (rankVal.isValid())
      initialRank = rankVal.toInt();

    bool ok;
    int rank =
        QInputDialog::getInt(this, i18n("Set Favourite Rank"), i18n("Rank:"),
                             initialRank, 1, 10000, 1, &ok);
    if (!ok)
      return;

    SourceModel *sm = qobject_cast<SourceModel *>(model);
    SessionModel *sessionModel = qobject_cast<SessionModel *>(model);
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex sIdx = proxy->mapToSource(idx);
      QString id = model->data(sIdx, idRole).toString();
      if (sm) {
        sm->setFavouriteRank(id, rank);
      } else if (sessionModel) {
        sessionModel->setFavouriteRank(id, rank);
      }
    }
  });
}
