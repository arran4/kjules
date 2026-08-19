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

void SessionsProxyModel::setTextFilter(const QString &text) {
  m_textFilter = text;
  invalidateFilter();
}

void SessionsProxyModel::setStatusFilter(const QString &status) {
  m_statusFilter = status;
  invalidateFilter();
}

void SessionsProxyModel::setRepoFilter(const QString &repo) {
  m_repoFilter = repo;
  invalidateFilter();
}

bool SessionsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QString title = sourceModel()->data(index, SessionModel::ColTitle).toString();
  QString source = sourceModel()->data(index, SessionModel::SourceRole).toString();
  QString state = sourceModel()->data(index, SessionModel::ColState).toString();
  QString repo = sourceModel()->data(index, SessionModel::ColRepo).toString();

  bool textMatch = m_textFilter.isEmpty() || title.contains(m_textFilter, Qt::CaseInsensitive) ||
                   source.contains(m_textFilter, Qt::CaseInsensitive);

  bool statusMatch = m_statusFilter.isEmpty() || m_statusFilter == i18n("All") || state == m_statusFilter;

  bool repoMatch = m_repoFilter.isEmpty() || m_repoFilter == i18n("All Repos") || repo == m_repoFilter;

  return textMatch && statusMatch && repoMatch;
}

bool SessionsProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
  QVariant leftData = sourceModel()->data(source_left, Qt::UserRole);
  QVariant rightData = sourceModel()->data(source_right, Qt::UserRole);

  if (leftData.typeId() == QMetaType::QDateTime && rightData.typeId() == QMetaType::QDateTime) {
    return leftData.toDateTime() < rightData.toDateTime();
  }
  if (leftData.typeId() == QMetaType::Int && rightData.typeId() == QMetaType::Int) {
    return leftData.toInt() < rightData.toInt();
  }

  if (leftData.typeId() == QMetaType::QString && rightData.typeId() == QMetaType::QString) {
    return leftData.toString() < rightData.toString();
  }

  return source_left.row() < source_right.row();
}

SessionsWidget::SessionsWidget(const QString &filterSource, APIManager *apiManager, SessionModel *managedModel,
                               QWidget *parent)
    : QWidget(parent), m_apiManager(apiManager), m_managedModel(managedModel), m_filterSource(filterSource),
      m_sessionsLoaded(0), m_isRefreshing(false), m_pagesLoaded(0), m_isRefreshingAll(false), m_autoLoadGroup(nullptr) {

  m_model = new SessionModel(QStringLiteral("cached_all_sessions.json"), this);
  m_proxyModel = new SessionsProxyModel(this);
  m_proxyModel->setSourceModel(m_model);

  if (!m_filterSource.isEmpty()) {
    m_proxyModel->setTextFilter(m_filterSource);
  } else {
    m_model->loadSessions();
    m_nextPageToken = m_model->nextPageToken();
  }

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

  m_statusLabel->setText(i18n("Loaded %1 cached sessions.", m_model->rowCount()));
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
  if (!m_filterSource.isEmpty()) {
    m_searchEdit->setText(m_filterSource);
  }
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
  if (value == m_listView->verticalScrollBar()->maximum() && !m_nextPageToken.isEmpty() && !m_isRefreshing) {
    if (m_autoLoadGroup && m_autoLoadGroup->checkedAction() &&
        (m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_bottom") ||
         m_autoLoadGroup->checkedAction()->data().toString() == QStringLiteral("load_all"))) {
      resumeRefresh();
    }
  }
}

void SessionsWidget::focusFilter() {
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void SessionsWidget::unmanageSelectedSessions() {
  if (!m_managedModel)
    return;

  QModelIndexList selectedRows = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &idx : selectedRows) {
    QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
    QModelIndexList matches =
        m_managedModel->match(m_managedModel->index(0, 0), SessionModel::IdRole, id, 1, Qt::MatchExactly);
    if (!matches.isEmpty()) {
      m_managedModel->removeSession(matches.first().row());
    }
    Q_EMIT deleteRequested(id);
  }
  updateActionStates();
}

void SessionsWidget::onListViewDoubleClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;
  QJsonObject session = m_proxyModel->data(index, Qt::UserRole).toJsonObject();
  SessionWindow *sessionWindow = new SessionWindow(session, m_apiManager, m_managedModel, this);
  sessionWindow->show();
}

void SessionsWidget::showContextMenu(const QPoint &pos) {
  QModelIndex index = m_listView->indexAt(pos);
  if (!index.isValid())
    return;

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

  menu.addSeparator();

  QAction *openPrAction = menu.addAction(i18n("Open PR"));
  connect(openPrAction, &QAction::triggered, this, &SessionsWidget::openPrUrls);
  QAction *copyPrAction = menu.addAction(i18n("Copy PR URL"));
  connect(copyPrAction, &QAction::triggered, this, &SessionsWidget::copyPrUrls);

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
  QJsonObject rawData = m_proxyModel->data(idx, Qt::UserRole).toJsonObject();
  QJsonObject sourceContext = rawData.value(QStringLiteral("sourceContext")).toObject();
  QString provider = sourceContext.value(QStringLiteral("provider")).toString();
  QString owner = sourceContext.value(QStringLiteral("owner")).toString();
  QString repo = sourceContext.value(QStringLiteral("repository")).toString();

  if (provider == QLatin1String("github")) {
    return QStringLiteral("https://github.com/") + owner + QLatin1Char('/') + repo;
  } else if (provider == QLatin1String("gitlab")) {
    return QStringLiteral("https://gitlab.com/") + owner + QLatin1Char('/') + repo;
  } else if (provider == QLatin1String("bitbucket")) {
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
