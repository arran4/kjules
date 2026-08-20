#include "sessionswidget.h"
#include "apimanager.h"
#include "sessionmodel.h"
#include "sessionswindow.h"
#include "sessionwindow.h"
#include "utils.h"

#include <KLocalizedString>
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

SessionsProxyModel::SessionsProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

void SessionsProxyModel::setSourceFilter(const QString &source) {
  if (m_sourceFilter == source)
    return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  m_sourceFilter = source;
  endFilterChange();
#else
  m_sourceFilter = source;
  invalidateFilter();
#endif
}

QString SessionsProxyModel::sourceFilter() const { return m_sourceFilter; }

void SessionsProxyModel::setTextFilter(const QString &text) {
  if (m_textFilter == text)
    return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  m_textFilter = text;
  endFilterChange();
#else
  m_textFilter = text;
  invalidateFilter();
#endif
}

void SessionsProxyModel::setStatusFilter(const QString &status) {
  if (m_statusFilter == status)
    return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  m_statusFilter = status;
  endFilterChange();
#else
  m_statusFilter = status;
  invalidateFilter();
#endif
}

void SessionsProxyModel::setRepoFilter(const QString &repo) {
  if (m_repoFilter == repo)
    return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  m_repoFilter = repo;
  endFilterChange();
#else
  m_repoFilter = repo;
  invalidateFilter();
#endif
}

bool SessionsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex indexTitle = sourceModel()->index(source_row, SessionModel::ColTitle, source_parent);
  QString fullSource = sourceModel()->data(indexTitle, SessionModel::SourceRole).toString();

  if (!m_sourceFilter.isEmpty() && fullSource != m_sourceFilter) {
    return false;
  }

  QModelIndex indexOwner = sourceModel()->index(source_row, SessionModel::ColOwner, source_parent);
  QModelIndex indexRepo = sourceModel()->index(source_row, SessionModel::ColRepo, source_parent);
  QModelIndex indexState = sourceModel()->index(source_row, SessionModel::ColState, source_parent);

  QString title = sourceModel()->data(indexTitle, Qt::DisplayRole).toString();
  QString owner = sourceModel()->data(indexOwner, Qt::DisplayRole).toString();
  QString repo = sourceModel()->data(indexRepo, Qt::DisplayRole).toString();
  QString state = sourceModel()->data(indexState, Qt::DisplayRole).toString();

  bool textMatch = m_textFilter.isEmpty() || title.contains(m_textFilter, Qt::CaseInsensitive) ||
                   owner.contains(m_textFilter, Qt::CaseInsensitive) ||
                   repo.contains(m_textFilter, Qt::CaseInsensitive) ||
                   fullSource.contains(m_textFilter, Qt::CaseInsensitive);

  bool statusMatch =
      m_statusFilter.isEmpty() || m_statusFilter == i18n("All") || state.contains(m_statusFilter, Qt::CaseInsensitive);

  bool repoMatch = m_repoFilter.isEmpty() || m_repoFilter == i18n("All Repos") || repo == m_repoFilter;

  return textMatch && statusMatch && repoMatch && QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool SessionsProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
  QAbstractItemModel *m = sourceModel();
  if (qobject_cast<SessionModel *>(m)) {
    bool leftFav = m->data(source_left, SessionModel::FavouriteRole).toBool();
    bool rightFav = m->data(source_right, SessionModel::FavouriteRole).toBool();

    if (leftFav != rightFav) {
      if (sortOrder() == Qt::AscendingOrder) {
        return leftFav;
      } else {
        return !leftFav;
      }
    }
  }
  return QSortFilterProxyModel::lessThan(source_left, source_right);
}

SessionsWidget::SessionsWidget(const QString &filterSource, APIManager *apiManager, SessionModel *managedModel,
                               QWidget *parent)
    : QWidget(parent), m_apiManager(apiManager), m_managedModel(managedModel), m_filterSource(filterSource),
      m_sessionsLoaded(0), m_isRefreshing(false), m_pagesLoaded(0), m_isRefreshingAll(false), m_autoLoadGroup(nullptr) {

  m_model = new SessionModel(QStringLiteral("cached_all_sessions.json"), this);
  m_proxyModel = new SessionsProxyModel(this);
  m_proxyModel->setSourceModel(m_model);

  if (!m_filterSource.isEmpty()) {
    m_proxyModel->setSourceFilter(m_filterSource);
  }

  m_model->loadSessions();
  m_nextPageToken = m_model->nextPageToken();

  setupUi();

  if (!m_nextPageToken.isEmpty()) {
    Q_EMIT canResumeChanged(true);
  }

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionsReceived, this, &SessionsWidget::onSessionsReceived);
    connect(m_apiManager, &APIManager::sessionsRefreshFinished, this, &SessionsWidget::onSessionsRefreshFinished);
    connect(m_apiManager, &APIManager::sessionReloaded, this, [this](const QJsonObject &session) {
      m_model->updateSession(session);
      m_model->saveSessions();
    });
  }

  m_statusLabel->setText(i18n("Loaded %1 cached sessions.", m_proxyModel->rowCount()));
}

SessionsWidget::~SessionsWidget() {}

SessionsProxyModel *SessionsWidget::proxyModel() const { return m_proxyModel; }
SessionModel *SessionsWidget::model() const { return m_model; }
QTreeView *SessionsWidget::listView() const { return m_listView; }

void SessionsWidget::setAutoLoadBehavior(QActionGroup *autoLoadGroup) { m_autoLoadGroup = autoLoadGroup; }
void SessionsWidget::setAutoFollowOnRefresh(bool autoFollow) { m_autoFollow = autoFollow; }

void SessionsWidget::setupUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  setupFilters(layout);

  m_listView = new QTreeView(this);
  m_listView->setModel(m_proxyModel);
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  setupListView();

  layout->addWidget(m_listView);

  QHBoxLayout *statusLayout = new QHBoxLayout();
  m_progressBar = new QProgressBar(this);
  m_progressBar->hide();
  m_cancelBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("process-stop")), QString(), this);
  m_cancelBtn->setToolTip(i18n("Cancel Refresh"));
  m_cancelBtn->hide();
  connect(m_cancelBtn, &QPushButton::clicked, this, &SessionsWidget::cancelRefresh);

  m_statusLabel = new QLabel(this);

  statusLayout->addWidget(m_progressBar);
  statusLayout->addWidget(m_cancelBtn);
  statusLayout->addWidget(m_statusLabel);
  statusLayout->addStretch();
  layout->addLayout(statusLayout);
}

void SessionsWidget::setupFilters(QVBoxLayout *layout) {
  QHBoxLayout *filterLayout = new QHBoxLayout();
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(i18n("Search title or source..."));
  connect(m_searchEdit, &QLineEdit::textChanged, m_proxyModel, &SessionsProxyModel::setTextFilter);
  filterLayout->addWidget(m_searchEdit);

  QComboBox *statusCombo = new QComboBox(this);
  statusCombo->addItems({i18n("All"), QStringLiteral("PENDING"), JulesStatus::IN_PROGRESS, QStringLiteral("COMPLETED"),
                         QStringLiteral("FAILED"), JulesStatus::CANCELED});
  connect(statusCombo, &QComboBox::currentTextChanged, m_proxyModel, &SessionsProxyModel::setStatusFilter);
  filterLayout->addWidget(statusCombo);

  m_repoCombo = new QComboBox(this);
  m_repoCombo->addItem(i18n("All Repos"));
  connect(m_repoCombo, &QComboBox::currentTextChanged, m_proxyModel, &SessionsProxyModel::setRepoFilter);
  filterLayout->addWidget(m_repoCombo);

  layout->addLayout(filterLayout);

  connect(m_model, &SessionModel::dataChanged, this, &SessionsWidget::updateRepoFilterList);
  connect(m_model, &SessionModel::rowsInserted, this, &SessionsWidget::updateRepoFilterList);
  connect(m_model, &SessionModel::modelReset, this, &SessionsWidget::updateRepoFilterList);
}

void SessionsWidget::setupListView() {
  m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_listView->setSortingEnabled(true);
  m_listView->setRootIsDecorated(false);

  m_listView->header()->setMinimumSectionSize(80);
  m_listView->header()->resizeSection(SessionModel::ColTitle, SessionModel::DefaultTitleWidth);
  m_listView->header()->resizeSection(SessionModel::ColState, 100);
  m_listView->header()->resizeSection(SessionModel::ColChangeSet, 80);
  m_listView->header()->resizeSection(SessionModel::ColPR, 80);
  m_listView->header()->resizeSection(SessionModel::ColUpdatedAt, 150);

  m_listView->sortByColumn(SessionModel::ColTitle, Qt::AscendingOrder);

  connect(m_listView->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &SessionsWidget::onVerticalScrollBarValueChanged);

  QAction *listDeleteAction = new QAction(i18n("Unmanage Session"), m_listView);
  listDeleteAction->setShortcut(QKeySequence::Delete);
  listDeleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(listDeleteAction, &QAction::triggered, this, &SessionsWidget::unmanageSelectedSessions);
  m_listView->addAction(listDeleteAction);

  connect(m_listView, &QTreeView::customContextMenuRequested, this, &SessionsWidget::showContextMenu);
  connect(m_listView, &QTreeView::doubleClicked, this, &SessionsWidget::onListViewDoubleClicked);
  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SessionsWidget::updateActionStates);
}

void SessionsWidget::onVerticalScrollBarValueChanged(int value) {
  if (m_autoLoadGroup && m_autoLoadGroup->checkedAction() &&
      (m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("auto_bottom") ||
       m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_all"))) {
    QScrollBar *vBar = m_listView->verticalScrollBar();
    if (value >= vBar->maximum() - 5 && !m_isRefreshing && !m_nextPageToken.isEmpty()) {
      resumeRefresh();
    }
  }
}

void SessionsWidget::focusFilter() {
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void SessionsWidget::watchSelectedSessions() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QModelIndex sourceIndex = m_proxyModel->mapToSource(idx);
    QJsonObject sessionData = m_model->getSession(sourceIndex.row());
    Q_EMIT watchRequested(sessionData);
  }
  updateActionStates();
}

void SessionsWidget::archiveSelectedSessions() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    Q_EMIT archiveRequested(id);
  }
  updateActionStates();
}

void SessionsWidget::unmanageSelectedSessions() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    Q_EMIT deleteRequested(id);
  }
  updateActionStates();
}

void SessionsWidget::onListViewDoubleClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;
  QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
  QJsonObject session = m_model->getSession(sourceIndex.row());
  QString id = session.value(QStringLiteral("id")).toString();
  bool isManaged = m_managedModel && m_managedModel->contains(id);
  SessionWindow *sessionWindow = new SessionWindow(session, m_apiManager, isManaged, this);
  sessionWindow->show();
}

void SessionsWidget::showContextMenu(const QPoint &pos) {
  QModelIndex index = m_listView->indexAt(pos);
  if (!index.isValid())
    return;

  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  if (!selectedRows.contains(index)) {
    m_listView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    selectedRows = m_listView->selectionModel()->selectedRows();
  }

  QMenu menu(this);

  QAction *openSessionAction = menu.addAction(i18n("Open Jules Session"));
  connect(openSessionAction, &QAction::triggered, this, &SessionsWidget::openSessionUrls);
  QAction *copySessionAction = menu.addAction(i18n("Copy Jules Session URL"));
  connect(copySessionAction, &QAction::triggered, this, &SessionsWidget::copySessionUrls);
  QAction *copyJulesIdAction = menu.addAction(i18n("Copy Jules ID"));
  connect(copyJulesIdAction, &QAction::triggered, this, &SessionsWidget::copyJulesIds);

  menu.addSeparator();

  QAction *openSourceAction = menu.addAction(i18n("Open Source Repository"));
  connect(openSourceAction, &QAction::triggered, this, &SessionsWidget::openSourceUrls);
  QAction *copySourceAction = menu.addAction(i18n("Copy Source URL"));
  connect(copySourceAction, &QAction::triggered, this, &SessionsWidget::copySourceUrls);

  bool hasPr = false;
  for (const QModelIndex &idx : selectedRows) {
    QString prUrl = m_proxyModel->data(idx, SessionModel::PrUrlRole).toString();
    if (!prUrl.isEmpty() && prUrl != QLatin1StringView("undefined")) {
      hasPr = true;
      break;
    }
  }

  if (hasPr) {
    menu.addSeparator();
    QAction *openPrAction = menu.addAction(i18n("Open PR"));
    connect(openPrAction, &QAction::triggered, this, &SessionsWidget::openPrUrls);
    QAction *copyPrAction = menu.addAction(i18n("Copy PR URL"));
    connect(copyPrAction, &QAction::triggered, this, &SessionsWidget::copyPrUrls);
  }

  menu.addSeparator();

  QAction *reloadAction = menu.addAction(i18n("Reload Sessions"));
  connect(reloadAction, &QAction::triggered, this, &SessionsWidget::reloadSelectedSessions);

  menu.addSeparator();

  QMenu *favMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("emblem-favorite")), i18n("Favourite"));
  QAction *toggleAction = favMenu->addAction(i18n("Toggle Favourite"));
  connect(toggleAction, &QAction::triggered, this, &SessionsWidget::toggleFavourite);
  QAction *incAction = favMenu->addAction(i18n("Increase Rank"));
  connect(incAction, &QAction::triggered, this, &SessionsWidget::increaseFavouriteRank);
  QAction *decAction = favMenu->addAction(i18n("Decrease Rank"));
  connect(decAction, &QAction::triggered, this, &SessionsWidget::decreaseFavouriteRank);
  QAction *setAction = favMenu->addAction(i18n("Set Rank..."));
  connect(setAction, &QAction::triggered, this, &SessionsWidget::setFavouriteRank);

  menu.addSeparator();

  bool allManaged = true;
  bool allUnmanaged = true;
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    if (m_managedModel && m_managedModel->contains(id)) {
      allUnmanaged = false;
    } else {
      allManaged = false;
    }
  }

  QAction *watchAction = menu.addAction(QIcon::fromTheme(QStringLiteral("visibility")), i18n("Follow Session"));
  watchAction->setEnabled(allUnmanaged);
  connect(watchAction, &QAction::triggered, this, &SessionsWidget::watchSelectedSessions);

  QAction *archiveAction = menu.addAction(QIcon::fromTheme(QStringLiteral("archive")), i18n("Archive Session"));
  archiveAction->setEnabled(allManaged);
  connect(archiveAction, &QAction::triggered, this, &SessionsWidget::archiveSelectedSessions);

  QAction *deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Unmanage Session"));
  deleteAction->setEnabled(allManaged);
  connect(deleteAction, &QAction::triggered, this, &SessionsWidget::unmanageSelectedSessions);

  menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

void SessionsWidget::openSessionUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  int count = 0;
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    if (!id.isEmpty()) {
      QDesktopServices::openUrl(QUrl(QStringLiteral("https://jules.google.com/session/") + id));
      count++;
    }
  }
  m_statusLabel->setText(i18n("Opened %1 session URLs", count));
}

void SessionsWidget::copySessionUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  QStringList urls;
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    if (!id.isEmpty()) {
      urls.append(QStringLiteral("https://jules.google.com/session/") + id);
    }
  }
  QGuiApplication::clipboard()->setText(urls.join(QLatin1Char('\n')));
  m_statusLabel->setText(i18n("Session URLs copied to clipboard."));
}

QString SessionsWidget::getSourceUrl(const QModelIndex &idx) const {
  QString provider = m_proxyModel->data(idx, SessionModel::ProviderRole).toString();
  QString owner = m_proxyModel->data(m_proxyModel->index(idx.row(), SessionModel::ColOwner)).toString();
  QString repo = m_proxyModel->data(m_proxyModel->index(idx.row(), SessionModel::ColRepo)).toString();

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
}

void SessionsWidget::openSourceUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  int count = 0;
  for (const QModelIndex &idx : selectedRows) {
    QString urlStr = getSourceUrl(idx);
    if (!urlStr.isEmpty()) {
      QDesktopServices::openUrl(QUrl(urlStr));
      count++;
    }
  }
  m_statusLabel->setText(i18n("Opened %1 source URLs", count));
}

void SessionsWidget::copySourceUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  QStringList urls;
  for (const QModelIndex &idx : selectedRows) {
    QString urlStr = getSourceUrl(idx);
    if (!urlStr.isEmpty()) {
      urls.append(urlStr);
    }
  }
  if (!urls.isEmpty()) {
    QGuiApplication::clipboard()->setText(urls.join(QLatin1Char('\n')));
    m_statusLabel->setText(i18n("Source URLs copied to clipboard."));
  } else {
    m_statusLabel->setText(i18n("No valid source URLs to copy."));
  }
}

void SessionsWidget::openPrUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  int count = 0;
  for (const QModelIndex &idx : selectedRows) {
    QString prUrl = m_proxyModel->data(idx, SessionModel::PrUrlRole).toString();
    if (!prUrl.isEmpty() && prUrl != QLatin1StringView("undefined")) {
      QDesktopServices::openUrl(QUrl(prUrl));
      count++;
    }
  }
  m_statusLabel->setText(i18n("Opened %1 PR URLs", count));
}

void SessionsWidget::copyPrUrls() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  QStringList urls;
  for (const QModelIndex &idx : selectedRows) {
    QString prUrl = m_proxyModel->data(idx, SessionModel::PrUrlRole).toString();
    if (!prUrl.isEmpty() && prUrl != QLatin1StringView("undefined")) {
      urls.append(prUrl);
    }
  }
  QGuiApplication::clipboard()->setText(urls.join(QLatin1Char('\n')));
  m_statusLabel->setText(i18n("PR URLs copied to clipboard."));
}

void SessionsWidget::reloadSelectedSessions() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    m_apiManager->reloadSession(id);
  }
  m_statusLabel->setText(i18n("Reloading %1 sessions...", selectedRows.size()));
}

void SessionsWidget::copyJulesIds() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  QStringList ids;
  for (const QModelIndex &idx : selectedRows) {
    ids.append(m_proxyModel->data(idx, SessionModel::IdRole).toString());
  }
  QGuiApplication::clipboard()->setText(ids.join(QLatin1Char('\n')));
  m_statusLabel->setText(i18n("Jules IDs copied to clipboard."));
}

void SessionsWidget::toggleFavourite() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QModelIndex sourceIndex = m_proxyModel->mapToSource(idx);
    QString id = m_model->data(sourceIndex, SessionModel::IdRole).toString();
    m_model->toggleFavourite(id);
  }
}

void SessionsWidget::updateActionStates() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  if (selectedRows.isEmpty()) {
    Q_EMIT actionStatesChanged(false, false, false);
    return;
  }

  bool allManaged = true;
  bool allUnmanaged = true;
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    if (m_managedModel && m_managedModel->contains(id)) {
      allUnmanaged = false;
    } else {
      allManaged = false;
    }
  }

  Q_EMIT actionStatesChanged(allUnmanaged, allManaged, allManaged);
}

void SessionsWidget::refreshSessions() {
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
  Q_EMIT canResumeChanged(false);

  m_progressBar->show();
  m_cancelBtn->show();
  m_statusLabel->setText(i18n("Refreshing sessions (Page %1)...", m_pagesLoaded));
  m_apiManager->listSessions();
}

void SessionsWidget::resumeRefresh() {
  if (m_isRefreshing || !m_apiManager || m_nextPageToken.isEmpty()) {
    return;
  }

  m_isRefreshing = true;
  m_pagesLoaded++;
  m_progressBar->show();
  m_cancelBtn->show();
  m_statusLabel->setText(i18n("Loading page %1...", m_pagesLoaded));
  Q_EMIT canResumeChanged(false);
  m_apiManager->listSessions(m_nextPageToken);
}

void SessionsWidget::loadRemainingRefresh() {
  m_isRefreshingAll = true;
  resumeRefresh();
}

void SessionsWidget::cancelRefresh() {
  if (m_apiManager) {
    m_apiManager->cancelListSessions();
  }
  m_isRefreshing = false;
  m_isRefreshingAll = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(i18n("Refresh cancelled. Loaded %1 sessions.", m_sessionsLoaded));
  Q_EMIT canResumeChanged(!m_nextPageToken.isEmpty());
}

void SessionsWidget::onSessionsReceived(const QJsonArray &sessions, const QString &nextPageToken) {
  int added = m_model->addSessions(sessions);
  m_sessionsLoaded += added;
  m_nextPageToken = nextPageToken;
  m_model->setNextPageToken(nextPageToken);
  m_progressBar->setFormat(i18n("%1 sessions loaded", m_sessionsLoaded));
  m_statusLabel->setText(i18n("Loading page %1... Loaded %2 sessions total.", m_pagesLoaded, m_sessionsLoaded));

  if (m_managedModel && m_autoFollow) {
    for (const QJsonValue &sessionValue : sessions) {
      const QJsonObject obj = sessionValue.toObject();
      if (!m_filterSource.isEmpty()) {
        QString sessionSource =
            obj.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
        if (sessionSource.isEmpty()) {
          sessionSource = obj.value(QStringLiteral("source")).toString();
        }
        if (sessionSource != m_filterSource) {
          continue;
        }
      }
      const QString state = obj.value(QStringLiteral("state")).toString();
      if (state == JulesStatus::IN_PROGRESS || state == JulesStatus::AWAITING_USER_FEEDBACK ||
          state == QStringLiteral("WAITING_APPROVAL")) {
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (!m_managedModel->contains(id)) {
          Q_EMIT watchRequested(obj);
        }
      }
    }
  }
}

void SessionsWidget::updateRepoFilterList() {
  QString currentSelection = m_repoCombo->currentText();
  m_repoCombo->blockSignals(true);
  m_repoCombo->clear();
  m_repoCombo->addItem(i18n("All Repos"));

  QSet<QString> uniqueRepos;
  for (int i = 0; i < m_model->rowCount(); ++i) {
    if (!m_filterSource.isEmpty()) {
      QString src = m_model->data(m_model->index(i, SessionModel::ColTitle), SessionModel::SourceRole).toString();
      if (src != m_filterSource) {
        continue;
      }
    }
    QString repo = m_model->data(m_model->index(i, SessionModel::ColRepo), Qt::DisplayRole).toString();
    if (!repo.isEmpty()) {
      uniqueRepos.insert(repo);
    }
  }

  QStringList sortedRepos = uniqueRepos.values();
  sortedRepos.sort(Qt::CaseInsensitive);
  m_repoCombo->addItems(sortedRepos);

  int index = m_repoCombo->findText(currentSelection);
  if (index != -1) {
    m_repoCombo->setCurrentIndex(index);
  } else {
    m_repoCombo->setCurrentIndex(0);
    m_proxyModel->setRepoFilter(i18n("All Repos"));
  }
  m_repoCombo->blockSignals(false);
}

void SessionsWidget::onSessionsRefreshFinished() {
  m_isRefreshing = false;

  if (m_isRefreshingAll && !m_nextPageToken.isEmpty()) {
    resumeRefresh();
    return;
  }

  m_isRefreshingAll = false;
  m_progressBar->hide();
  m_cancelBtn->hide();
  m_statusLabel->setText(i18n("Finished refreshing. Loaded %1 sessions.", m_sessionsLoaded));
  Q_EMIT canResumeChanged(!m_nextPageToken.isEmpty());

  if (m_filterSource.isEmpty()) {
    m_model->saveSessions();
  }

  if (!m_nextPageToken.isEmpty() && m_autoLoadGroup && m_autoLoadGroup->checkedAction() &&
      m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_all")) {
    resumeRefresh();
  }
}

void SessionsWidget::applyFavouriteAction(std::function<void(const QString &)> action) {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QModelIndex sourceIndex = m_proxyModel->mapToSource(idx);
    QString id = m_model->data(sourceIndex, SessionModel::IdRole).toString();
    action(id);
  }
}

void SessionsWidget::increaseFavouriteRank() {
  applyFavouriteAction([this](const QString &id) { m_model->increaseFavouriteRank(id); });
}

void SessionsWidget::decreaseFavouriteRank() {
  applyFavouriteAction([this](const QString &id) { m_model->decreaseFavouriteRank(id); });
}

void SessionsWidget::setFavouriteRank() {
  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  if (selectedRows.isEmpty())
    return;

  QModelIndex firstSourceIndex = m_proxyModel->mapToSource(selectedRows.first());
  QVariant currentRankVal = m_model->data(firstSourceIndex, SessionModel::FavouriteRole);
  int initialRank = currentRankVal.isValid() ? currentRankVal.toInt() : 1;

  bool ok;
  int rank = QInputDialog::getInt(this, i18n("Set Favourite Rank"), i18n("Rank:"), initialRank, 1, 10000, 1, &ok);
  if (!ok)
    return;

  applyFavouriteAction([this, rank](const QString &id) { m_model->setFavouriteRank(id, rank); });
}
