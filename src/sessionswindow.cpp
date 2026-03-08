#include "sessionswindow.h"
#include "apimanager.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sessionwindow.h"

#include <KLocalizedString>
#include <QAction>
#include <QLabel>
#include <QTreeView>
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
#include <QHeaderView>
#include <QDialog>
#include <QTextBrowser>
#include <QJsonDocument>
#include <QActionGroup>
#include <QScrollBar>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QDesktopServices>
#include <QUrl>

SessionsProxyModel::SessionsProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

void SessionsProxyModel::setTextFilter(const QString &text) {
  m_textFilter = text;
  invalidateFilter();
}

void SessionsProxyModel::setStatusFilter(const QString &status) {
  m_statusFilter = status;
  invalidateFilter();
}

bool SessionsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  QModelIndex indexTitle = sourceModel()->index(source_row, SessionModel::ColTitle, source_parent);
  QModelIndex indexOwner = sourceModel()->index(source_row, SessionModel::ColOwner, source_parent);
  QModelIndex indexRepo = sourceModel()->index(source_row, SessionModel::ColRepo, source_parent);
  QModelIndex indexState = sourceModel()->index(source_row, SessionModel::ColState, source_parent);

  QString title = sourceModel()->data(indexTitle, Qt::DisplayRole).toString();
  QString owner = sourceModel()->data(indexOwner, Qt::DisplayRole).toString();
  QString repo = sourceModel()->data(indexRepo, Qt::DisplayRole).toString();
  QString state = sourceModel()->data(indexState, Qt::DisplayRole).toString();
  QString fullSource = sourceModel()->data(indexTitle, SessionModel::SourceRole).toString();

  bool textMatch = m_textFilter.isEmpty() ||
                   title.contains(m_textFilter, Qt::CaseInsensitive) ||
                   owner.contains(m_textFilter, Qt::CaseInsensitive) ||
                   repo.contains(m_textFilter, Qt::CaseInsensitive) ||
                   fullSource.contains(m_textFilter, Qt::CaseInsensitive);
  bool statusMatch = m_statusFilter.isEmpty() || m_statusFilter == i18n("All") || state.contains(m_statusFilter, Qt::CaseInsensitive);

  return textMatch && statusMatch && QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

SessionsWindow::SessionsWindow(const QString &filterSource,
                               APIManager *apiManager, QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(apiManager),
      m_filterSource(filterSource), m_sessionsLoaded(0), m_isRefreshing(false),
      m_pagesLoaded(0), m_isRefreshingAll(false) {

  m_model = new SessionModel(this);
  m_proxyModel = new SessionsProxyModel(this);
  m_proxyModel->setSourceModel(m_model);

  if (!m_filterSource.isEmpty()) {
    m_proxyModel->setTextFilter(m_filterSource);
    setWindowTitle(i18n("Sessions for %1", m_filterSource));
  } else {
    setWindowTitle(i18n("All Sessions"));
    m_model->loadSessions();
    m_nextPageToken = m_model->nextPageToken();
  }

  setupUi();

  if (!m_nextPageToken.isEmpty() && m_resumeAction) {
    m_resumeAction->setEnabled(true);
  }

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionsReceived, this,
            &SessionsWindow::onSessionsReceived);
    connect(m_apiManager, &APIManager::sessionsRefreshFinished, this,
            &SessionsWindow::onSessionsRefreshFinished);
  }

  m_statusLabel->setText(
      i18n("Loaded %1 cached sessions.", m_model->rowCount()));
}

SessionsWindow::~SessionsWindow() {}

void SessionsWindow::setupUi() {
  setAttribute(Qt::WA_DeleteOnClose);
  resize(800, 600);

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);
  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  QHBoxLayout *filterLayout = new QHBoxLayout();
  QLineEdit *searchEdit = new QLineEdit(this);
  searchEdit->setPlaceholderText(i18n("Search title or source..."));
  if (!m_filterSource.isEmpty()) {
    searchEdit->setText(m_filterSource);
  }
  connect(searchEdit, &QLineEdit::textChanged, m_proxyModel, &SessionsProxyModel::setTextFilter);
  filterLayout->addWidget(searchEdit);

  QComboBox *statusCombo = new QComboBox(this);
  statusCombo->addItems({i18n("All"), QStringLiteral("PENDING"), QStringLiteral("IN_PROGRESS"), QStringLiteral("COMPLETED"), QStringLiteral("FAILED"), QStringLiteral("CANCELED")});
  connect(statusCombo, &QComboBox::currentTextChanged, m_proxyModel, &SessionsProxyModel::setStatusFilter);
  filterLayout->addWidget(statusCombo);

  layout->addLayout(filterLayout);

  QTabWidget *tabWidget = new QTabWidget(this);

  m_listView = new QTreeView(this);
  m_listView->setModel(m_proxyModel);
  // Remove SessionDelegate if it's meant for a list view, or adjust it
  // m_listView->setItemDelegate(new SessionDelegate(this));
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  // Set some treeview properties
  m_listView->setSortingEnabled(true);
  m_listView->setRootIsDecorated(false);

  // Header configuration
  m_listView->header()->setMinimumSectionSize(100);
  m_listView->header()->resizeSection(SessionModel::ColTitle, 250);
  m_listView->header()->resizeSection(SessionModel::ColState, 100);
  m_listView->header()->resizeSection(SessionModel::ColChangeSet, 80);
  m_listView->header()->resizeSection(SessionModel::ColPR, 80);
  m_listView->header()->resizeSection(SessionModel::ColUpdatedAt, 150);
  m_listView->sortByColumn(SessionModel::ColUpdatedAt, Qt::DescendingOrder);

  connect(m_listView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
    if (m_autoLoadGroup && m_autoLoadGroup->checkedAction() &&
        m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("auto_bottom")) {
      QScrollBar *vBar = m_listView->verticalScrollBar();
      if (value >= vBar->maximum() - 5 && !m_isRefreshing && !m_nextPageToken.isEmpty()) {
        resumeRefresh();
      }
    }
  });

  tabWidget->addTab(m_listView, i18n("All sessions"));

  connect(
      m_listView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_listView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *openSessionUrlAction = menu.addAction(i18n("Open Session URL"));
          QAction *copySessionUrlAction = menu.addAction(i18n("Copy Session URL"));
          menu.addSeparator();
          QAction *openSourceUrlAction = menu.addAction(i18n("Open Source URL"));
          QAction *copySourceUrlAction = menu.addAction(i18n("Copy Source URL"));

          connect(openSessionUrlAction, &QAction::triggered, [this, index]() {
            QString id = m_proxyModel->data(m_proxyModel->index(index.row(), SessionModel::ColId)).toString();
            QString urlStr = QStringLiteral("https://jules.google.com/sessions/") + id;
            QDesktopServices::openUrl(QUrl(urlStr));
            m_statusLabel->setText(i18n("Opening session %1", id));
          });

          connect(copySessionUrlAction, &QAction::triggered, [this, index]() {
            QString id = m_proxyModel->data(m_proxyModel->index(index.row(), SessionModel::ColId)).toString();
            QGuiApplication::clipboard()->setText(
                QStringLiteral("https://jules.google.com/sessions/") + id);
            m_statusLabel->setText(i18n("Session URL copied to clipboard."));
          });

          auto getSourceUrl = [this, index]() -> QString {
            QString provider = m_proxyModel->data(index, SessionModel::ProviderRole).toString();
            QString owner = m_proxyModel->data(m_proxyModel->index(index.row(), SessionModel::ColOwner)).toString();
            QString repo = m_proxyModel->data(m_proxyModel->index(index.row(), SessionModel::ColRepo)).toString();
            if (provider == QStringLiteral("github")) {
              return QStringLiteral("https://github.com/") + owner + QLatin1Char('/') + repo;
            } else if (provider == QStringLiteral("gitlab")) {
              return QStringLiteral("https://gitlab.com/") + owner + QLatin1Char('/') + repo;
            } else if (provider == QStringLiteral("bitbucket")) {
              return QStringLiteral("https://bitbucket.org/") + owner + QLatin1Char('/') + repo;
            } else if (!provider.isEmpty()) {
              return QStringLiteral("https://") + provider + QStringLiteral(".com/") + owner + QLatin1Char('/') + repo;
            }
            return QString();
          };

          connect(openSourceUrlAction, &QAction::triggered, [this, getSourceUrl]() {
            QString urlStr = getSourceUrl();
            if (!urlStr.isEmpty()) {
              QDesktopServices::openUrl(QUrl(urlStr));
              m_statusLabel->setText(i18n("Opening source %1", urlStr));
            } else {
              m_statusLabel->setText(i18n("Invalid source URL."));
            }
          });

          connect(copySourceUrlAction, &QAction::triggered, [this, getSourceUrl]() {
            QString urlStr = getSourceUrl();
            if (!urlStr.isEmpty()) {
              QGuiApplication::clipboard()->setText(urlStr);
              m_statusLabel->setText(i18n("Source URL copied to clipboard."));
            } else {
              m_statusLabel->setText(i18n("Invalid source URL."));
            }
          });

          QString prUrl = m_proxyModel->data(index, SessionModel::PrUrlRole).toString();
          if (!prUrl.isEmpty()) {
            menu.addSeparator();
            QAction *openPrUrlAction = menu.addAction(i18n("Open PR URL"));
            QAction *copyPrUrlAction = menu.addAction(i18n("Copy PR URL"));

            connect(openPrUrlAction, &QAction::triggered, [this, prUrl]() {
              QDesktopServices::openUrl(QUrl(prUrl));
              m_statusLabel->setText(i18n("Opening PR %1", prUrl));
            });

            connect(copyPrUrlAction, &QAction::triggered, [this, prUrl]() {
              QGuiApplication::clipboard()->setText(prUrl);
              m_statusLabel->setText(i18n("PR URL copied to clipboard."));
            });
          }

          menu.addSeparator();
          QAction *copyIdAction = menu.addAction(i18n("Copy Jules ID"));
          connect(copyIdAction, &QAction::triggered, [this, index]() {
            QString id = m_proxyModel->data(index, SessionModel::IdRole).toString();
            QGuiApplication::clipboard()->setText(id);
            m_statusLabel->setText(i18n("Jules ID copied to clipboard."));
          });

          menu.exec(m_listView->mapToGlobal(pos));
        }
      });

  connect(m_listView, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &index) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
            QJsonObject rawData = m_model->getSession(sourceIndex.row());

            QDialog *detailDialog = new QDialog(this);
            detailDialog->setAttribute(Qt::WA_DeleteOnClose);
            detailDialog->setWindowTitle(i18n("Session Details"));
            detailDialog->resize(600, 400);

            QVBoxLayout *dlgLayout = new QVBoxLayout(detailDialog);
            QTextBrowser *textBrowser = new QTextBrowser(detailDialog);
            QJsonDocument doc(rawData);
            textBrowser->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

            dlgLayout->addWidget(textBrowser);
            detailDialog->exec();
          });

  layout->addWidget(tabWidget);

  // Actions
  QAction *refreshAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
  connect(refreshAction, &QAction::triggered, this, &SessionsWindow::refreshSessions);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"), refreshAction);
  actionCollection()->setDefaultShortcut(refreshAction, QKeySequence(Qt::Key_F5));

  m_resumeAction = new QAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("Load More"), this);
  connect(m_resumeAction, &QAction::triggered, this, &SessionsWindow::resumeRefresh);
  actionCollection()->addAction(QStringLiteral("resume_refresh"), m_resumeAction);
  m_resumeAction->setEnabled(false);

  // Menu
  QMenu *fileMenu = new QMenu(i18n("File"), this);
  fileMenu->addAction(refreshAction);
  fileMenu->addAction(m_resumeAction);
  QAction *quitAction = new QAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Close"), this);
  connect(quitAction, &QAction::triggered, this, &SessionsWindow::close);
  fileMenu->addAction(quitAction);
  menuBar()->addMenu(fileMenu);

  QMenu *prefsMenu = new QMenu(i18n("Preferences"), this);
  QMenu *autoLoadMenu = prefsMenu->addMenu(i18n("Auto Load Behavior"));

  m_autoLoadGroup = new QActionGroup(this);
  QAction *manualAction = new QAction(i18n("Manual"), this);
  manualAction->setCheckable(true);
  manualAction->setData(QStringLiteral("manual"));
  m_autoLoadGroup->addAction(manualAction);

  QAction *loadAllAction = new QAction(i18n("Load All On Refresh"), this);
  loadAllAction->setCheckable(true);
  loadAllAction->setData(QStringLiteral("load_all"));
  m_autoLoadGroup->addAction(loadAllAction);

  QAction *autoBottomAction = new QAction(i18n("Auto-Load when at bottom"), this);
  autoBottomAction->setCheckable(true);
  autoBottomAction->setData(QStringLiteral("auto_bottom"));
  m_autoLoadGroup->addAction(autoBottomAction);

  autoLoadMenu->addActions(m_autoLoadGroup->actions());
  menuBar()->addMenu(prefsMenu);

  QMenu *viewMenu = new QMenu(i18n("View"), this);
  QMenu *columnsMenu = viewMenu->addMenu(i18n("Columns"));

  KConfigGroup config(KSharedConfig::openConfig(), "SessionsWindow");
  QString autoLoadMode = config.readEntry("AutoLoadMode", "manual");
  for (QAction *action : m_autoLoadGroup->actions()) {
    if (action->data().toString() == autoLoadMode) {
      action->setChecked(true);
      break;
    }
  }
  if (!m_autoLoadGroup->checkedAction()) {
    manualAction->setChecked(true);
  }

  connect(m_autoLoadGroup, &QActionGroup::triggered, [this](QAction *action) {
    KConfigGroup config(KSharedConfig::openConfig(), "SessionsWindow");
    config.writeEntry("AutoLoadMode", action->data().toString());
    config.sync();
  });

  auto addColumnToggle = [this, columnsMenu, &config](const QString &label, int colIndex) {
    QAction *action = new QAction(label, this);
    action->setCheckable(true);

    QString key = QStringLiteral("ShowColumn_%1").arg(colIndex);
    bool isVisible = config.readEntry(key, true);
    action->setChecked(isVisible);
    m_listView->header()->setSectionHidden(colIndex, !isVisible);

    connect(action, &QAction::toggled, [this, colIndex](bool checked) {
      m_listView->header()->setSectionHidden(colIndex, !checked);
      KConfigGroup config(KSharedConfig::openConfig(), "SessionsWindow");
      config.writeEntry(QStringLiteral("ShowColumn_%1").arg(colIndex), checked);
      config.sync();
    });

    columnsMenu->addAction(action);
  };

  addColumnToggle(i18n("Title"), SessionModel::ColTitle);
  addColumnToggle(i18n("State"), SessionModel::ColState);
  addColumnToggle(i18n("Change Set"), SessionModel::ColChangeSet);
  addColumnToggle(i18n("PR"), SessionModel::ColPR);
  addColumnToggle(i18n("Updated At"), SessionModel::ColUpdatedAt);
  addColumnToggle(i18n("Created At"), SessionModel::ColCreatedAt);
  addColumnToggle(i18n("Owner"), SessionModel::ColOwner);
  addColumnToggle(i18n("Repo"), SessionModel::ColRepo);
  addColumnToggle(i18n("ID"), SessionModel::ColId);

  menuBar()->addMenu(viewMenu);

  // Toolbar
  QToolBar *toolBar = addToolBar(i18n("Main Toolbar"));
  toolBar->setObjectName(QStringLiteral("mainToolBar"));
  toolBar->addAction(refreshAction);
  toolBar->addAction(m_resumeAction);

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

  if (m_autoLoadGroup && m_autoLoadGroup->checkedAction() &&
      m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_all")) {
    m_isRefreshingAll = true;
  } else {
    m_isRefreshingAll = false;
  }

  m_sessionsLoaded = 0;
  m_pagesLoaded = 1;
  m_nextPageToken.clear();
  m_resumeAction->setEnabled(false);

  m_progressBar->show();
  m_cancelBtn->show();
  m_statusLabel->setText(i18n("Refreshing sessions (Page %1)...", m_pagesLoaded));
  m_apiManager->listSessions();
}

void SessionsWindow::resumeRefresh() {
  if (m_isRefreshing || !m_apiManager || m_nextPageToken.isEmpty()) {
    return;
  }

  m_isRefreshing = true;
  m_pagesLoaded++;
  m_progressBar->show();
  m_cancelBtn->show();
  m_statusLabel->setText(i18n("Loading page %1...", m_pagesLoaded));
  m_resumeAction->setEnabled(false);
  m_apiManager->listSessions(m_nextPageToken);
}

void SessionsWindow::cancelRefresh() {
  if (m_apiManager) {
    m_apiManager->cancelListSessions();
  }
  m_isRefreshing = false;
  m_isRefreshingAll = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(i18n("Refresh cancelled. Loaded %1 sessions.", m_sessionsLoaded));
  m_resumeAction->setEnabled(!m_nextPageToken.isEmpty());
}

void SessionsWindow::onSessionsReceived(const QJsonArray &sessions, const QString &nextPageToken) {
  int added = m_model->addSessions(sessions);
  m_sessionsLoaded += added;
  m_nextPageToken = nextPageToken;
  m_model->setNextPageToken(nextPageToken);
  m_progressBar->setFormat(i18n("%1 sessions loaded", m_sessionsLoaded));
  m_statusLabel->setText(i18n("Loading page %1... Loaded %2 sessions total.", m_pagesLoaded, m_sessionsLoaded));
}

void SessionsWindow::onSessionsRefreshFinished() {
  m_isRefreshing = false;

  if (m_isRefreshingAll && !m_nextPageToken.isEmpty()) {
    resumeRefresh();
    return;
  }

  m_isRefreshingAll = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(
      i18n("Finished refreshing. Loaded %1 sessions.", m_sessionsLoaded));
  m_resumeAction->setEnabled(!m_nextPageToken.isEmpty());
  if (m_filterSource.isEmpty()) {
    m_model->saveSessions();
  }

  if (!m_nextPageToken.isEmpty() && m_autoLoadGroup->checkedAction() &&
      m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_all")) {
    resumeRefresh();
  }
}
