#include "mainwindow.h"
#include "apimanager.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "errorsmodel.h"
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
#include <KToolBar>
#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QMenu>
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

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(new APIManager(this)),
      m_sessionModel(new SessionModel(this)),
      m_sourceModel(new SourceModel(this)),
      m_draftsModel(new DraftsModel(this)),
      m_errorsModel(new ErrorsModel(this)), m_isRefreshingSources(false),
      m_sourcesLoadedCount(0), m_sourcesAddedCount(0), m_pagesLoadedCount(0),
      m_sessionRefreshTimer(new QTimer(this)) {
  setupUi();

  connect(m_sessionRefreshTimer, &QTimer::timeout, this,
          &MainWindow::updateSessionStats);
  m_sessionRefreshTimer->start(60000); // 1 minute
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
  connect(m_apiManager, &APIManager::sessionCreationFailed, this,
          &MainWindow::onSessionCreationFailed);
  connect(m_apiManager, &APIManager::sessionCreated,
          [this](const QJsonObject &session) {
            m_sessionModel->addSession(session);
            updateSessionStats();
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
  QTimer::singleShot(0, this, [this]() { refreshSources(); });
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  QTabWidget *tabWidget = new QTabWidget(this);

  // Sources View
  m_sourceView = new QTreeView(this);

  QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
  proxyModel->setSourceModel(m_sourceModel);
  proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_sourceView->setModel(proxyModel);
  m_sourceView->setSortingEnabled(true);
  m_sourceView->sortByColumn(SourceModel::ColName, Qt::AscendingOrder);
  m_sourceView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sourceView->header()->setStretchLastSection(true);

  m_sourceView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sourceView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sourceView->indexAt(pos);
        if (index.isValid()) {
          m_sourceView->setCurrentIndex(index);
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
                  [this, sourceIndex]() { onSourceActivated(sourceIndex); });

          menu.addAction(m_viewSessionsAction);
          menu.addAction(m_showPastNewSessionsAction);
          menu.addAction(m_viewRawDataAction);
          menu.addAction(m_openUrlAction);
          menu.addAction(m_copyUrlAction);

          menu.exec(m_sourceView->mapToGlobal(pos));
        }
      });
  connect(m_sourceView, &QTreeView::doubleClicked, this,
          &MainWindow::onSourceActivated);
  tabWidget->addTab(m_sourceView, i18n("Sources"));

  // Sessions View
  m_sessionView = new QTreeView(this);
  QSortFilterProxyModel *sessionProxyModel = new QSortFilterProxyModel(this);
  sessionProxyModel->setSourceModel(m_sessionModel);
  sessionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  sessionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_sessionView->setModel(sessionProxyModel);
  m_sessionView->setSortingEnabled(true);
  m_sessionView->sortByColumn(SessionModel::ColName, Qt::AscendingOrder);
  m_sessionView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sessionView->header()->setStretchLastSection(true);
  m_sessionView->setItemDelegate(new SessionDelegate(this));
  m_sessionView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sessionView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          m_sessionView->setCurrentIndex(index);
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(
                  m_sessionView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

          QMenu menu;
          QAction *openUrlAction = menu.addAction(i18n("Open URL"));
          QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

          connect(openUrlAction, &QAction::triggered, [this, sourceIndex]() {
            QString id = m_sessionModel->data(sourceIndex, SessionModel::IdRole)
                             .toString();

            QRegularExpression idRegex(QStringLiteral("^[a-zA-Z0-9_-]+$"));
            if (!idRegex.match(id).hasMatch()) {
              updateStatus(i18n("Invalid session ID format."));
              return;
            }

            // Placeholder URL logic
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, sourceIndex]() {
            QString id = m_sessionModel->data(sourceIndex, SessionModel::IdRole)
                             .toString();

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
  connect(m_sessionView, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &index) {
            const QSortFilterProxyModel *proxy =
                qobject_cast<const QSortFilterProxyModel *>(
                    m_sessionView->model());
            QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
            onSessionActivated(sourceIndex);
          });

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

  // Errors View
  m_errorsView = new QListView(this);
  m_errorsView->setModel(m_errorsModel);
  m_errorsView->setItemDelegate(new DraftDelegate(
      this)); // Reusing DraftDelegate for simple display or create custom
  m_errorsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_errorsView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_errorsView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *editAction = menu.addAction(i18n("Edit / Modify"));
          QAction *rawTranscriptAction = menu.addAction(i18n("Raw Transcript"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));

          connect(editAction, &QAction::triggered,
                  [this, index]() { onErrorActivated(index); });

          connect(rawTranscriptAction, &QAction::triggered, [this, index]() {
            QJsonObject errorData = m_errorsModel->getError(index.row());
            KXmlGuiWindow *rawWindow = new KXmlGuiWindow(this);
            rawWindow->setAttribute(Qt::WA_DeleteOnClose);
            rawWindow->setWindowTitle(i18n("Raw Transcript"));

            QTextBrowser *textBrowser = new QTextBrowser(rawWindow);
            QJsonDocument doc(errorData);
            textBrowser->setPlainText(
                QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

            rawWindow->setCentralWidget(textBrowser);
            rawWindow->resize(600, 400);
            rawWindow->show();
          });

          connect(deleteAction, &QAction::triggered, [this, index]() {
            if (QMessageBox::question(this, i18n("Delete Error"),
                                      i18n("Are you sure?")) ==
                QMessageBox::Yes) {
              m_errorsModel->removeError(index.row());
            }
          });

          menu.exec(m_errorsView->mapToGlobal(pos));
        }
      });
  connect(m_errorsView, &QListView::doubleClicked, this,
          &MainWindow::onErrorActivated);

  tabWidget->addTab(m_errorsView, i18n("Errors"));

  mainLayout->addWidget(tabWidget);

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
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new KStatusNotifierItem(this);
  m_trayIcon->setIconByPixmap(QIcon(QStringLiteral(":/icons/kjules-tray.png")));
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

  m_showFullSessionListAction =
      new QAction(i18n("Show Full Session List"), this);
  actionCollection()->addAction(QStringLiteral("show_full_session_list"),
                                m_showFullSessionListAction);
  connect(m_showFullSessionListAction, &QAction::triggered, this, [this]() {
    // Replaces old refresh sessions / view full list functionality
    KXmlGuiWindow *sessionsWindow = new KXmlGuiWindow(this);
    sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
    sessionsWindow->setWindowTitle(i18n("Full Session List"));

    QTreeView *treeView = new QTreeView(sessionsWindow);
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(treeView);
    proxyModel->setSourceModel(m_sessionModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    treeView->setModel(proxyModel);
    treeView->setSortingEnabled(true);
    treeView->sortByColumn(SessionModel::ColName, Qt::AscendingOrder);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->header()->setStretchLastSection(true);
    treeView->setItemDelegate(new SessionDelegate(treeView));

    connect(treeView, &QTreeView::doubleClicked, this,
            [this, proxyModel](const QModelIndex &filterIndex) {
              QModelIndex sourceIndex = proxyModel->mapToSource(filterIndex);
              onSessionActivated(sourceIndex);
            });

    sessionsWindow->setCentralWidget(treeView);
    sessionsWindow->resize(600, 400);
    sessionsWindow->show();
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

  QAction *toggleWindowAction =
      new QAction(QIcon::fromTheme(QStringLiteral("window-minimize")),
                  i18n("Minimize to Tray"), this);
  connect(toggleWindowAction, &QAction::triggered, this,
          &MainWindow::toggleWindowVisibility);
  actionCollection()->addAction(QStringLiteral("toggle_window"),
                                toggleWindowAction);
  KGlobalAccel::setGlobalShortcut(toggleWindowAction, QKeySequence());

  m_viewSessionsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-list-details")),
                  i18n("View Sessions"), this);
  actionCollection()->addAction(QStringLiteral("view_sessions"),
                                m_viewSessionsAction);
  connect(m_viewSessionsAction, &QAction::triggered, this, [this]() {
    QModelIndex index = m_sourceView->currentIndex();
    if (!index.isValid())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
    QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
    QString id =
        m_sourceModel->data(sourceIndex, SourceModel::IdRole).toString();

    QSortFilterProxyModel *filterModel = new QSortFilterProxyModel(
        this); // Temp parent to avoid leak if we don't refactor everything
    filterModel->setSourceModel(m_sessionModel);
    filterModel->setFilterRole(SessionModel::SourceRole);
    filterModel->setFilterFixedString(id);

    KXmlGuiWindow *sessionsWindow = new KXmlGuiWindow(this);
    filterModel->setParent(sessionsWindow); // Reparent to window to avoid leak
    sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
    sessionsWindow->setWindowTitle(i18n("Sessions for %1", id));

    QTreeView *treeView = new QTreeView(sessionsWindow);
    treeView->setModel(filterModel);
    treeView->setSortingEnabled(true);
    treeView->sortByColumn(SessionModel::ColName, Qt::AscendingOrder);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->header()->setStretchLastSection(true);
    treeView->setItemDelegate(new SessionDelegate(treeView));

    connect(treeView, &QTreeView::doubleClicked, this,
            [this, filterModel](const QModelIndex &filterIndex) {
              QModelIndex srcIdx = filterModel->mapToSource(filterIndex);
              onSessionActivated(srcIdx);
            });
    sessionsWindow->setCentralWidget(treeView);
    sessionsWindow->resize(600, 400);
    sessionsWindow->show();
  });

  m_showPastNewSessionsAction =
      new QAction(i18n("Show past new sessions"), this);
  actionCollection()->addAction(QStringLiteral("show_past_new_sessions"),
                                m_showPastNewSessionsAction);
  connect(m_showPastNewSessionsAction, &QAction::triggered, this, [this]() {
    QModelIndex index = m_sourceView->currentIndex();
    if (!index.isValid())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
    QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
    QString id =
        m_sourceModel->data(sourceIndex, SourceModel::IdRole).toString();

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
    SessionModel *localModel = new SessionModel(sessionsWindow);
    localModel->setSessions(filteredSessions);
    sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
    sessionsWindow->setWindowTitle(i18n("Past New Sessions for %1", id));

    QTreeView *treeView = new QTreeView(sessionsWindow);
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(treeView);
    proxyModel->setSourceModel(localModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    treeView->setModel(proxyModel);
    treeView->setSortingEnabled(true);
    treeView->sortByColumn(SessionModel::ColName, Qt::AscendingOrder);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->header()->setStretchLastSection(true);
    treeView->setItemDelegate(new SessionDelegate(treeView));

    connect(
        treeView, &QTreeView::doubleClicked, this,
        [this, localModel, proxyModel](const QModelIndex &filterIndex) {
          QModelIndex sourceIndex = proxyModel->mapToSource(filterIndex);
          QString sessId =
              localModel->data(sourceIndex, SessionModel::IdRole).toString();
          m_apiManager->getSession(sessId);
          updateStatus(i18n("Fetching details for session %1...", sessId));
        });
    sessionsWindow->setCentralWidget(treeView);
    sessionsWindow->resize(600, 400);
    sessionsWindow->show();
  });

  m_viewRawDataAction = new QAction(i18n("View Raw Data"), this);
  actionCollection()->addAction(QStringLiteral("view_raw_data"),
                                m_viewRawDataAction);
  connect(m_viewRawDataAction, &QAction::triggered, this, [this]() {
    QModelIndex index = m_sourceView->currentIndex();
    if (!index.isValid())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
    QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

    QJsonObject rawData =
        m_sourceModel->data(sourceIndex, SourceModel::RawDataRole)
            .toJsonObject();
    KXmlGuiWindow *rawWindow = new KXmlGuiWindow(this);
    rawWindow->setAttribute(Qt::WA_DeleteOnClose);
    rawWindow->setWindowTitle(i18n("Raw Data for Source"));
    QTextBrowser *textBrowser = new QTextBrowser(rawWindow);
    QJsonDocument doc(rawData);
    textBrowser->setPlainText(
        QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    rawWindow->setCentralWidget(textBrowser);
    rawWindow->resize(600, 400);
    rawWindow->show();
  });

  m_openUrlAction = new QAction(i18n("Open URL"), this);
  actionCollection()->addAction(QStringLiteral("open_url"), m_openUrlAction);
  connect(m_openUrlAction, &QAction::triggered, this, [this]() {
    QModelIndex index = m_sourceView->currentIndex();
    if (!index.isValid())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
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
                 QStringLiteral(".com/") + owner + QLatin1Char('/') + repo;
      }
    } else {
      urlStr = id;
    }

    if (!urlStr.isEmpty()) {
      QDesktopServices::openUrl(QUrl(urlStr));
      updateStatus(i18n("Opening source %1", id));
    } else {
      updateStatus(i18n("Invalid source ID for opening URL."));
    }
  });

  m_copyUrlAction = new QAction(i18n("Copy URL"), this);
  actionCollection()->addAction(QStringLiteral("copy_url"), m_copyUrlAction);
  connect(m_copyUrlAction, &QAction::triggered, this, [this]() {
    QModelIndex index = m_sourceView->currentIndex();
    if (!index.isValid())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
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
                 QStringLiteral(".com/") + owner + QLatin1Char('/') + repo;
      }
    } else {
      urlStr = id;
    }

    if (!urlStr.isEmpty()) {
      QGuiApplication::clipboard()->setText(urlStr);
      updateStatus(i18n("URL copied to clipboard."));
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

void MainWindow::onSessionCreationFailed(const QJsonObject &request,
                                         const QJsonObject &response,
                                         const QString &errorString) {
  QMessageBox msgBox(this);
  msgBox.setWindowTitle(i18n("Session Creation Failed"));
  msgBox.setText(i18n("Failed to create session:\n%1", errorString));

  QPushButton *continueBtn =
      msgBox.addButton(i18n("Continue Editing"), QMessageBox::ActionRole);
  QPushButton *saveBtn =
      msgBox.addButton(i18n("Save to Drafts"), QMessageBox::ActionRole);
  QPushButton *ignoreBtn =
      msgBox.addButton(i18n("Ignore"), QMessageBox::RejectRole);
  msgBox.setDefaultButton(ignoreBtn);

  msgBox.exec();

  if (msgBox.clickedButton() == continueBtn) {
    bool hasApiKey = !m_apiManager->apiKey().isEmpty();
    NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
    dialog.setInitialData(request);

    connect(&dialog, &NewSessionDialog::createSessionRequested, this,
            &MainWindow::onSessionCreated);
    connect(&dialog, &NewSessionDialog::saveDraftRequested, this,
            &MainWindow::onDraftSaved);
    dialog.exec();
  } else if (msgBox.clickedButton() == saveBtn) {
    m_draftsModel->addDraft(request);
    updateStatus(i18n("Saved to drafts."));
  } else {
    // Ignore, save to errors
    m_errorsModel->addError(request, response, errorString);
    updateStatus(i18n("Error saved."));
  }
}

void MainWindow::onErrorActivated(const QModelIndex &index) {
  QJsonObject errorData = m_errorsModel->getError(index.row());
  QJsonObject request = errorData.value(QStringLiteral("request")).toObject();

  bool hasApiKey = !m_apiManager->apiKey().isEmpty();
  NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
  dialog.setInitialData(request);

  connect(&dialog, &NewSessionDialog::createSessionRequested,
          [this, index](const QStringList &sources, const QString &p,
                        const QString &a) {
            onSessionCreated(sources, p, a);
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
  // Map index from proxy to source
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());
  QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

  QString sourceName =
      m_sourceModel->data(sourceIndex, SourceModel::NameRole).toString();
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
