#include "sourcewindow.h"
#include "apimanager.h"
#include "blockedtreemodel.h"
#include "clickablelabel.h"
#include "draftdelegate.h"
#include "errorsmodel.h"
#include "queuedelegate.h"
#include "queuemodel.h"
#include "sessionmodel.h"
#include "sessionswidget.h"
#include "sessionwindow.h"
#include "sourcemodel.h"
#include "sourcestatuswidget.h"
#include <KActionCollection>
#include <QLabel>
#include <QStatusBar>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

class SourceSessionFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SourceSessionFilterProxyModel(const QString &sourceId, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_sourceId(sourceId) {}

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
    if (!sourceModel())
      return false;
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    QString src = sourceModel()->data(idx, SessionModel::SourceRole).toString();
    return src == m_sourceId;
  }

private:
  QString m_sourceId;
};

SourceWindow::SourceWindow(const QString &sourceId, SourceModel *sourceModel, SessionModel *sessionModel,
                           SessionModel *archiveModel, QueueModel *queueModel, ErrorsModel *errorsModel,
                           BlockedTreeModel *blockedTreeModel, APIManager *apiManager, QWidget *parent)
    : KXmlGuiWindow(parent), m_sourceId(sourceId), m_sourceModel(sourceModel), m_sessionModel(sessionModel),
      m_archiveModel(archiveModel), m_queueModel(queueModel), m_errorsModel(errorsModel),
      m_blockedTreeModel(blockedTreeModel), m_apiManager(apiManager), m_tabWidget(nullptr), m_sessionsWidget(nullptr),
      m_autoFollowCheckBox(nullptr), m_concurrencySpinBox(nullptr), m_defaultBranchesList(nullptr),
      m_rawDataEdit(nullptr) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Source: %1").arg(sourceId));
  resize(800, 600);

  setupUi();

  QAction *newSessionAction = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), tr("New Session"), this);
  actionCollection()->addAction(QStringLiteral("new_session"), newSessionAction);
  connect(newSessionAction, &QAction::triggered, this, [this]() { Q_EMIT newSessionRequested(m_sourceId); });

  QAction *refreshSessionsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh Sessions"), this);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"), refreshSessionsAction);
  actionCollection()->setDefaultShortcut(refreshSessionsAction, QKeySequence(Qt::Key_F5));
  connect(refreshSessionsAction, &QAction::triggered, this, [this]() {
    if (m_sessionsWidget) {
      m_sessionsWidget->refreshSessions();
    }
  });

  QAction *favAction = new QAction(QIcon::fromTheme(QStringLiteral("emblem-favorite")), tr("Toggle Favourite"), this);
  actionCollection()->addAction(QStringLiteral("toggle_favourite"), favAction);
  connect(favAction, &QAction::triggered, this, [this]() { m_sourceModel->toggleFavourite(m_sourceId); });

  QAction *closeAction = new QAction(i18n("Close"), this);
  actionCollection()->addAction(QStringLiteral("file_close"), closeAction);
  connect(closeAction, &QAction::triggered, this, &SourceWindow::close);
  actionCollection()->setDefaultShortcut(closeAction, QKeySequence(Qt::CTRL | Qt::Key_W));

  setupGUI(ToolBar | Keys | StatusBar | Create, QStringLiteral(":/kxmlgui6/org.kde.kjules/sourcewindowui.rc"));
}

SourceWindow::~SourceWindow() = default;

QString SourceWindow::generateGithubIssuePrompt(const QString &sourceName, const QString &owner,
                                                const QString &repository, const QJsonObject &issue,
                                                const QJsonArray &comments) {
  QString prompt;
  int issueNumber = issue.value(QStringLiteral("number")).toInt();
  QString title = issue.value(QStringLiteral("title")).toString();
  QString canonicalUrl = QStringLiteral("https://github.com/%1/%2/issues/%3").arg(owner, repository).arg(issueNumber);

  prompt += QStringLiteral("Implement GitHub issue #%1: %2\n\n").arg(issueNumber).arg(title);
  prompt += QStringLiteral("Repository/source: %1\n").arg(sourceName);
  prompt += QStringLiteral("Issue: #%1\n").arg(issueNumber);
  prompt += QStringLiteral("Reference URL: %1\n").arg(canonicalUrl);

  if (issue.contains(QStringLiteral("user"))) {
    QJsonObject user = issue.value(QStringLiteral("user")).toObject();
    prompt += QStringLiteral("Author: @%1\n").arg(user.value(QStringLiteral("login")).toString());
  }

  prompt += QStringLiteral("State: %1\n").arg(issue.value(QStringLiteral("state")).toString());

  if (issue.contains(QStringLiteral("labels"))) {
    QJsonArray labelsArr = issue.value(QStringLiteral("labels")).toArray();
    QStringList labels;
    for (const QJsonValue &lv : labelsArr) {
      if (lv.isObject()) {
        labels.append(lv.toObject().value(QStringLiteral("name")).toString());
      } else {
        labels.append(lv.toString());
      }
    }
    if (!labels.isEmpty()) {
      prompt += QStringLiteral("Labels: %1\n").arg(labels.join(QStringLiteral(", ")));
    }
  }

  if (issue.contains(QStringLiteral("assignees"))) {
    QJsonArray assigneesArr = issue.value(QStringLiteral("assignees")).toArray();
    QStringList assignees;
    for (const QJsonValue &av : assigneesArr) {
      assignees.append(av.toObject().value(QStringLiteral("login")).toString());
    }
    if (!assignees.isEmpty()) {
      prompt += QStringLiteral("Assignees: %1\n").arg(assignees.join(QStringLiteral(", ")));
    }
  }

  if (issue.contains(QStringLiteral("milestone")) && !issue.value(QStringLiteral("milestone")).isNull()) {
    QJsonObject milestone = issue.value(QStringLiteral("milestone")).toObject();
    prompt += QStringLiteral("Milestone: %1\n").arg(milestone.value(QStringLiteral("title")).toString());
  }

  if (issue.contains(QStringLiteral("created_at"))) {
    prompt += QStringLiteral("Created: %1\n").arg(issue.value(QStringLiteral("created_at")).toString());
  }

  if (issue.contains(QStringLiteral("updated_at"))) {
    prompt += QStringLiteral("Updated: %1\n").arg(issue.value(QStringLiteral("updated_at")).toString());
  }

  prompt += QStringLiteral("\nIMPORTANT: This GitHub repository and/or issue may be private. Your execution\n"
                           "environment may not be able to access the GitHub URL. Do not rely on being\n"
                           "able to open the issue. The issue content and discussion available to kjules\n"
                           "have been copied below and should be treated as the issue context.\n\n");

  prompt += QStringLiteral("## Issue description\n\n");
  QString body = issue.value(QStringLiteral("body")).toString();
  if (body.isEmpty()) {
    body = QStringLiteral("*No description provided.*");
  }
  prompt += body + QStringLiteral("\n\n");

  if (!comments.isEmpty()) {
    prompt += QStringLiteral("## Discussion\n\n");
    for (const QJsonValue &cv : comments) {
      QJsonObject comment = cv.toObject();
      QString author = QStringLiteral("unknown");
      if (comment.contains(QStringLiteral("user"))) {
        author = comment.value(QStringLiteral("user")).toObject().value(QStringLiteral("login")).toString();
      }
      QString timestamp = comment.value(QStringLiteral("created_at")).toString();
      prompt += QStringLiteral("### @%1 — %2\n\n").arg(author, timestamp);
      prompt += comment.value(QStringLiteral("body")).toString() + QStringLiteral("\n\n");
    }
  }

  prompt += QStringLiteral("## Task\n\n"
                           "Implement the issue described above in the selected source repository.\n\n"
                           "Use the checked-out source as the authority for the current implementation.\n"
                           "Reconcile the requested change with the current code rather than assuming\n"
                           "the issue was written against exactly the current revision.\n\n"
                           "Run the relevant tests and add/update tests for the changed behaviour.\n");

  return prompt;
}

void SourceWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);

  m_statusLabel = new QLabel(this);
  m_unseenErrorLabel = new ClickableLabel(this);
  m_unseenErrorLabel->hide();

  statusBar()->addWidget(m_statusLabel);
  statusBar()->addWidget(m_unseenErrorLabel);

  connect(m_unseenErrorLabel, &ClickableLabel::clicked, this, [this]() {
    for (int i = 0; i < m_tabWidget->count(); ++i) {
      if (m_tabWidget->widget(i)->objectName() == QStringLiteral("queuedBlockedTab")) {
        m_tabWidget->setCurrentIndex(i);
        QTabWidget *subTabWidget = m_tabWidget->widget(i)->findChild<QTabWidget *>();
        if (subTabWidget) {
          for (int j = 0; j < subTabWidget->count(); ++j) {
            if (subTabWidget->widget(j)->objectName() == QStringLiteral("blockedTab")) {
              subTabWidget->setCurrentIndex(j);
              break;
            }
          }
        }
        break;
      }
    }
  });

  auto updateUnseenErrors = [this]() {
    if (!m_errorsModel)
      return;
    int count = 0;
    for (int i = 0; i < m_errorsModel->rowCount(); ++i) {
      QModelIndex idx = m_errorsModel->index(i, 0);
      if (m_errorsModel->data(idx, ErrorsModel::SourceIdRole).toString() == m_sourceId &&
          m_errorsModel->data(idx, ErrorsModel::UnseenRole).toBool()) {
        count++;
      }
    }
    if (count > 0) {
      m_unseenErrorLabel->setText(i18np("[Error: %1]", "[Errors: %1]", count));
      m_unseenErrorLabel->show();
    } else {
      m_unseenErrorLabel->hide();
    }
  };

  if (m_errorsModel) {
    connect(m_errorsModel, &ErrorsModel::unseenCountChanged, this, updateUnseenErrors);
    connect(m_errorsModel, &ErrorsModel::dataChanged, this,
            [updateUnseenErrors](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
              if (roles.contains(ErrorsModel::SeenRole) || roles.contains(ErrorsModel::UnseenRole) || roles.isEmpty()) {
                updateUnseenErrors();
              }
            });
    updateUnseenErrors();
  }

  connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
    if (m_tabWidget->widget(index)->objectName() == QStringLiteral("queuedBlockedTab")) {
      if (m_errorsModel) {
        for (int i = 0; i < m_errorsModel->rowCount(); ++i) {
          QModelIndex idx = m_errorsModel->index(i, 0);
          if (m_errorsModel->data(idx, ErrorsModel::SourceIdRole).toString() == m_sourceId) {
            m_errorsModel->markSeen(i);
          }
        }
      }
    }
  });

  connect(this, &SourceWindow::statusMessage, this, [this](const QString &msg) { m_statusLabel->setText(msg); });

  layout->addWidget(m_tabWidget);

  setupFollowingTab();
  setupArchivedTab();
  setupQueuedBlockedTab();
  setupSessionsTab();
  setupSettingsTab();
  setupRawDataTab();
  setupGithubIssuesTab();
  setupGithubPRsTab();

  m_tabWidget->setCurrentIndex(0);
}

void SourceWindow::setupFollowingTab() {
  QWidget *followingTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(followingTab);

  QTreeView *view = new QTreeView(followingTab);
  SourceSessionFilterProxyModel *proxy = new SourceSessionFilterProxyModel(m_sourceId, view);
  proxy->setSourceModel(m_sessionModel);
  view->setModel(proxy);
  view->setSortingEnabled(true);
  view->setSelectionBehavior(QAbstractItemView::SelectRows);
  view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view->header()->setMinimumSectionSize(80);
  view->header()->resizeSection(SessionModel::ColTitle, SessionModel::DefaultTitleWidth);
  view->header()->setStretchLastSection(true);
  view->sortByColumn(SessionModel::ColTitle, Qt::AscendingOrder);

  connect(view, &QTreeView::doubleClicked, this, [this, proxy](const QModelIndex &index) {
    if (!index.isValid() || !m_sessionModel)
      return;
    QModelIndex sourceIdx = proxy->mapToSource(index);
    QJsonObject session = m_sessionModel->getSession(sourceIdx.row());
    SessionWindow *win = new SessionWindow(session, m_apiManager, m_errorsModel, true, this);
    win->show();
  });

  layout->addWidget(view);
  m_tabWidget->addTab(followingTab, tr("Following"));
}

void SourceWindow::setupArchivedTab() {
  QWidget *archivedTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(archivedTab);

  QTreeView *view = new QTreeView(archivedTab);
  SourceSessionFilterProxyModel *proxy = new SourceSessionFilterProxyModel(m_sourceId, view);
  proxy->setSourceModel(m_archiveModel);
  view->setModel(proxy);
  view->setSortingEnabled(true);
  view->setSelectionBehavior(QAbstractItemView::SelectRows);
  view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view->header()->setMinimumSectionSize(80);
  view->header()->resizeSection(SessionModel::ColTitle, SessionModel::DefaultTitleWidth);
  view->header()->setStretchLastSection(true);
  view->sortByColumn(SessionModel::ColTitle, Qt::AscendingOrder);

  connect(view, &QTreeView::doubleClicked, this, [this, proxy](const QModelIndex &index) {
    if (!index.isValid() || !m_archiveModel)
      return;
    QModelIndex sourceIdx = proxy->mapToSource(index);
    QJsonObject session = m_archiveModel->getSession(sourceIdx.row());
    SessionWindow *win = new SessionWindow(session, m_apiManager, m_errorsModel, false, this);
    win->show();
  });

  layout->addWidget(view);
  m_tabWidget->addTab(archivedTab, tr("Archived"));
}

void SourceWindow::setupQueuedBlockedTab() {
  QWidget *queuedBlockedTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(queuedBlockedTab);

  QTabWidget *subTabWidget = new QTabWidget(queuedBlockedTab);

  // In Queue
  QWidget *queueTab = new QWidget(subTabWidget);
  QVBoxLayout *queueLayout = new QVBoxLayout(queueTab);
  QListView *queueView = new QListView(queueTab);
  queueView->setItemDelegate(new QueueDelegate(queueView));
  SourceFilterProxyModel *queueProxy = new SourceFilterProxyModel(m_sourceId, queueTab);
  queueProxy->setSourceModel(m_queueModel);
  queueView->setModel(queueProxy);
  queueLayout->addWidget(queueView);
  subTabWidget->addTab(queueTab, tr("In Queue"));

  // Blocked / Error
  QWidget *blockedTab = new QWidget(subTabWidget);
  QVBoxLayout *blockedLayout = new QVBoxLayout(blockedTab);
  blockedLayout->addWidget(new QLabel(tr("Errors:"), blockedTab));
  QListView *errorView = new QListView(blockedTab);
  errorView->setItemDelegate(new DraftDelegate(errorView));
  ErrorFilterProxyModel *errorProxy = new ErrorFilterProxyModel(m_sourceId, blockedTab);
  errorProxy->setSourceModel(m_errorsModel);
  errorView->setModel(errorProxy);
  blockedLayout->addWidget(errorView);

  blockedLayout->addWidget(new QLabel(tr("Blocked Items:"), blockedTab));
  QTreeView *blockedView = new QTreeView(blockedTab);
  blockedView->setHeaderHidden(true);
  BlockedErrorProxyModel *blockedProxy = new BlockedErrorProxyModel(m_sourceId, blockedTab);
  blockedProxy->setSourceModel(m_blockedTreeModel);
  blockedView->setModel(blockedProxy);
  blockedLayout->addWidget(blockedView);

  subTabWidget->addTab(blockedTab, tr("Blocked / Error"));

  layout->addWidget(subTabWidget);
  m_tabWidget->addTab(queuedBlockedTab, tr("Queued/Blocked"));
}

void SourceWindow::setupSessionsTab() {
  m_sessionsWidget = new SessionsWidget(m_sourceId, m_apiManager, m_sessionModel, m_errorsModel, this);
  connect(m_sessionsWidget, &SessionsWidget::watchRequested, this, [this](const QJsonObject &session) {
    if (m_sessionModel) {
      m_sessionModel->addSession(session);
      m_sessionModel->saveSessions();
    }
  });
  connect(m_sessionsWidget, &SessionsWidget::archiveRequested, this, [this](const QString &id) {
    if (m_sessionModel) {
      QModelIndexList matches =
          m_sessionModel->match(m_sessionModel->index(0, 0), SessionModel::IdRole, id, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        QJsonObject session = m_sessionModel->getSession(matches.first().row());
        m_sessionModel->removeSession(matches.first().row());
        m_sessionModel->saveSessions();
        if (m_archiveModel) {
          m_archiveModel->addSession(session);
          m_archiveModel->saveSessions();
        }
      }
    }
  });
  connect(m_sessionsWidget, &SessionsWidget::deleteRequested, this, [this](const QString &id) {
    if (m_sessionModel) {
      QModelIndexList matches =
          m_sessionModel->match(m_sessionModel->index(0, 0), SessionModel::IdRole, id, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        m_sessionModel->removeSession(matches.first().row());
        m_sessionModel->saveSessions();
      }
    }
  });
  m_tabWidget->addTab(m_sessionsWidget, tr("Sessions"));
}

void SourceWindow::setupSettingsTab() {
  QWidget *settingsTab = new QWidget(this);
  QFormLayout *formLayout = new QFormLayout(settingsTab);

  // Auto Follow
  m_autoFollowCheckBox = new QCheckBox(tr("Start Following New Sessions"), this);
  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  bool autoFollow = false;
  if (!matches.isEmpty()) {
    QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
    autoFollow = rawData.value(QStringLiteral("local_autoFollowNewSessions")).toBool(false);
  }
  m_autoFollowCheckBox->setChecked(autoFollow);
  if (m_sessionsWidget) {
    m_sessionsWidget->setAutoFollowOnRefresh(autoFollow);
  }

  connect(m_autoFollowCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
    m_sourceModel->setAutoFollow(m_sourceId, checked);
    if (m_sessionsWidget) {
      m_sessionsWidget->setAutoFollowOnRefresh(checked);
    }
  });
  formLayout->addRow(m_autoFollowCheckBox);

  // Concurrency Limit
  KConfigGroup sourceConfig(KSharedConfig::openConfig(), QStringLiteral("SourceConcurrency"));
  int currentLimit = sourceConfig.readEntry(m_sourceId, -1);
  m_concurrencySpinBox = new QSpinBox(this);
  m_concurrencySpinBox->setMinimum(-1);
  m_concurrencySpinBox->setMaximum(1000);
  m_concurrencySpinBox->setValue(currentLimit);
  m_concurrencySpinBox->setSpecialValueText(tr("Global Default"));
  connect(m_concurrencySpinBox, &QSpinBox::valueChanged, this, [this](int value) {
    KConfigGroup sourceConfig(KSharedConfig::openConfig(), QStringLiteral("SourceConcurrency"));
    if (value == -1) {
      sourceConfig.deleteEntry(m_sourceId);
    } else {
      sourceConfig.writeEntry(m_sourceId, value);
    }
    sourceConfig.sync();
    Q_EMIT queueProcessingRequested();
  });
  formLayout->addRow(tr("Concurrency Limit:"), m_concurrencySpinBox);

  // Default Branches
  QWidget *branchesWidget = new QWidget(this);
  QVBoxLayout *branchesLayout = new QVBoxLayout(branchesWidget);
  branchesLayout->setContentsMargins(0, 0, 0, 0);

  m_defaultBranchesList = new QListWidget(this);
  populateDefaultBranches();

  QHBoxLayout *branchesButtons = new QHBoxLayout();
  QPushButton *addBranchBtn = new QPushButton(tr("Add Branch"), this);
  QPushButton *removeBranchBtn = new QPushButton(tr("Remove Branch"), this);
  branchesButtons->addWidget(addBranchBtn);
  branchesButtons->addWidget(removeBranchBtn);
  branchesButtons->addStretch();

  connect(addBranchBtn, &QPushButton::clicked, this, [this]() {
    bool ok;
    QString branch =
        QInputDialog::getText(this, tr("Add Default Branch"), tr("Branch name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !branch.isEmpty()) {
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
        QStringList branches;

        if (rawData.contains(QStringLiteral("local_defaultBranches"))) {
          QJsonArray branchesArr = rawData.value(QStringLiteral("local_defaultBranches")).toArray();
          for (const QJsonValue &v : branchesArr) {
            branches.append(v.toString());
          }
        } else {
          branches = m_sourceModel->getEffectiveDefaultBranches(m_sourceId);
        }

        if (!branches.contains(branch)) {
          branches.append(branch);
          m_sourceModel->setDefaultBranches(m_sourceId, branches);
          populateDefaultBranches();
        }
      }
    }
  });
  connect(removeBranchBtn, &QPushButton::clicked, this, [this]() {
    auto items = m_defaultBranchesList->selectedItems();
    if (!items.isEmpty()) {
      QString branch = items.first()->text();
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
        QStringList branches;

        if (rawData.contains(QStringLiteral("local_defaultBranches"))) {
          QJsonArray branchesArr = rawData.value(QStringLiteral("local_defaultBranches")).toArray();
          for (const QJsonValue &v : branchesArr) {
            branches.append(v.toString());
          }
        } else {
          branches = m_sourceModel->getEffectiveDefaultBranches(m_sourceId);
        }

        branches.removeAll(branch);

        if (branches.isEmpty()) {
          m_sourceModel->clearDefaultBranches(m_sourceId);
        } else {
          m_sourceModel->setDefaultBranches(m_sourceId, branches);
        }
        populateDefaultBranches();
      }
    }
  });
  branchesLayout->addWidget(m_defaultBranchesList);
  branchesLayout->addLayout(branchesButtons);
  formLayout->addRow(tr("Default Branches:"), branchesWidget);

  m_tabWidget->addTab(settingsTab, tr("Settings"));
}

void SourceWindow::populateDefaultBranches() {
  m_defaultBranchesList->clear();
  QStringList branches = m_sourceModel->getEffectiveDefaultBranches(m_sourceId);
  for (const QString &b : branches) {
    m_defaultBranchesList->addItem(b);
  }
}

void SourceWindow::setupGithubIssuesTab() {
  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  if (matches.isEmpty()) {
    return;
  }

  QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
  QString owner = SourceModel::githubOwner(rawData);
  QString repo = SourceModel::githubRepository(rawData);
  if (owner.isEmpty() || repo.isEmpty()) {
    return;
  }
  QWidget *issuesTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(issuesTab);

  m_issuesView = new QTreeView(this);
  m_issuesModel = new QStandardItemModel(this);
  m_issuesModel->setHorizontalHeaderLabels({tr("Number"), tr("Title"), tr("State"), tr("User")});
  m_issuesView->setModel(m_issuesModel);
  m_issuesView->setAlternatingRowColors(true);
  m_issuesView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_issuesView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_issuesView->setSortingEnabled(true);
  m_issuesView->setContextMenuPolicy(Qt::ActionsContextMenu);

  m_createSessionFromIssueAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-new")), tr("Create Session..."), this);
  m_createSessionFromIssueAction->setEnabled(false);
  m_issuesView->addAction(m_createSessionFromIssueAction);

  connect(m_issuesView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          [this]() { m_createSessionFromIssueAction->setEnabled(m_issuesView->selectionModel()->hasSelection()); });

  connect(m_createSessionFromIssueAction, &QAction::triggered, this, [this, owner, repo]() {
    QModelIndexList selected = m_issuesView->selectionModel()->selectedRows();
    if (selected.isEmpty())
      return;
    int issueNumber = m_issuesModel->itemFromIndex(selected.first())->data(Qt::UserRole).toInt();

    m_createSessionFromIssueAction->setEnabled(false);
    m_createSessionFromIssueAction->setText(tr("Fetching..."));
    m_apiManager->fetchGithubIssueContext(m_sourceId, owner, repo, issueNumber);
  });

  connect(m_issuesView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
    if (index.isValid() && m_createSessionFromIssueAction->isEnabled()) {
      m_createSessionFromIssueAction->trigger();
    }
  });

  layout->addWidget(m_issuesView);

  connect(m_apiManager, &APIManager::githubIssuesReceived, this, &SourceWindow::onGithubIssuesReceived);
  connect(m_apiManager, &APIManager::githubIssueContextReceived, this, &SourceWindow::onGithubIssueContextReceived);
  connect(m_apiManager, &APIManager::githubIssueContextFailed, this, &SourceWindow::onGithubIssueContextFailed);

  m_issuesStateCombo = new QComboBox(this);
  QComboBox *stateCombo = m_issuesStateCombo;
  stateCombo->addItem(tr("Open"), QStringLiteral("open"));
  stateCombo->addItem(tr("Closed"), QStringLiteral("closed"));
  stateCombo->addItem(tr("All"), QStringLiteral("all"));

  QPushButton *refreshBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh Issues"), this);

  connect(m_apiManager, &APIManager::githubAvailabilityChanged, this, [refreshBtn, stateCombo](bool available) {
    refreshBtn->setEnabled(available);
    stateCombo->setEnabled(available);
  });

  connect(refreshBtn, &QPushButton::clicked, this, [this, owner, repo, stateCombo]() {
    m_apiManager->fetchGithubIssues(m_sourceId, owner, repo, stateCombo->currentData().toString());
  });

  connect(stateCombo, &QComboBox::currentIndexChanged, this, [this, owner, repo, stateCombo]() {
    QString currentState = stateCombo->currentData().toString();
    QString cacheKey = QStringLiteral("local_githubIssues_") + currentState;

    QModelIndexList matches =
        m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
    if (!matches.isEmpty()) {
      QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
      if (rawData.contains(cacheKey)) {
        onGithubIssuesReceived(m_sourceId, rawData.value(cacheKey).toArray());
      } else if (currentState == QStringLiteral("open") && rawData.contains(QStringLiteral("local_githubIssues"))) {
        // Fallback to legacy cache
        onGithubIssuesReceived(m_sourceId, rawData.value(QStringLiteral("local_githubIssues")).toArray());
      } else {
        m_issuesModel->removeRows(0, m_issuesModel->rowCount()); // clear until loaded
      }
    }

    // Always refresh network too if available
    m_apiManager->fetchGithubIssues(m_sourceId, owner, repo, currentState);
  });

  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->addWidget(new QLabel(tr("State:")));
  btnLayout->addWidget(stateCombo);
  btnLayout->addStretch();
  btnLayout->addWidget(refreshBtn);

  layout->addLayout(btnLayout);

  QString currentState = stateCombo->currentData().toString();
  QString cacheKey = QStringLiteral("local_githubIssues_") + currentState;
  if (rawData.contains(cacheKey)) {
    onGithubIssuesReceived(m_sourceId, rawData.value(cacheKey).toArray());
  } else if (rawData.contains(QStringLiteral("local_githubIssues"))) {
    // Fallback to legacy cache
    onGithubIssuesReceived(m_sourceId, rawData.value(QStringLiteral("local_githubIssues")).toArray());
  } else {
    m_apiManager->fetchGithubIssues(m_sourceId, owner, repo, currentState);
  }

  m_tabWidget->addTab(issuesTab, tr("GitHub Issues"));
}

void SourceWindow::setupGithubPRsTab() {
  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  if (matches.isEmpty()) {
    return;
  }

  QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
  QString owner = SourceModel::githubOwner(rawData);
  QString repo = SourceModel::githubRepository(rawData);
  if (owner.isEmpty() || repo.isEmpty()) {
    return;
  }
  QWidget *prsTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(prsTab);

  m_prsView = new QTreeView(this);
  m_prsModel = new QStandardItemModel(this);
  m_prsModel->setHorizontalHeaderLabels({tr("Number"), tr("Title"), tr("State"), tr("User")});
  m_prsView->setModel(m_prsModel);
  m_prsView->setAlternatingRowColors(true);
  m_prsView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_prsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_prsView->setSortingEnabled(true);
  layout->addWidget(m_prsView);

  if (rawData.contains(QStringLiteral("local_githubPRs"))) {
    onGithubPullRequestsReceived(m_sourceId, rawData.value(QStringLiteral("local_githubPRs")).toArray());
  }

  QPushButton *refreshBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh Pull Requests"), this);
  connect(refreshBtn, &QPushButton::clicked, this,
          [this, owner, repo]() { m_apiManager->fetchGithubPullRequests(m_sourceId, owner, repo); });

  connect(m_apiManager, &APIManager::githubPullRequestsReceived, this, &SourceWindow::onGithubPullRequestsReceived);
  connect(m_apiManager, &APIManager::githubAvailabilityChanged, this,
          [refreshBtn](bool available) { refreshBtn->setEnabled(available); });

  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  btnLayout->addWidget(refreshBtn);
  layout->addLayout(btnLayout);

  m_tabWidget->addTab(prsTab, tr("GitHub PRs"));
}

void SourceWindow::setupRawDataTab() {
  QWidget *rawDataTab = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(rawDataTab);

  m_rawDataEdit = new QTextEdit(this);

  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  if (!matches.isEmpty()) {
    QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
    QJsonDocument doc(rawData);
    m_rawDataEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
  }

  layout->addWidget(m_rawDataEdit);

  QPushButton *saveBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), tr("Save Raw Data"), this);
  connect(saveBtn, &QPushButton::clicked, this, [this]() {
    QJsonParseError parseError;
    QJsonDocument newDoc = QJsonDocument::fromJson(m_rawDataEdit->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
      QMessageBox::warning(this, tr("Invalid JSON"), tr("The JSON data is invalid: %1").arg(parseError.errorString()));
      return;
    }
    if (!newDoc.isObject()) {
      QMessageBox::warning(this, tr("Invalid JSON"), tr("The JSON data must be an object."));
      return;
    }
    QJsonObject obj = newDoc.object();

    // Protect Identity
    if (SourceModel::resourceName(obj) != m_sourceId) {
      QMessageBox::warning(this, tr("Identity Change Rejected"),
                           tr("Changing the source ID via raw data is not allowed."));
      return;
    }

    if (m_sourceModel->updateSourceRaw(m_sourceId, obj)) {
      QMessageBox::information(this, tr("Saved"), tr("Source settings saved successfully."));
    } else {
      QMessageBox::warning(this, tr("Save Failed"), tr("Could not save source settings."));
    }
  });
  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  btnLayout->addWidget(saveBtn);
  layout->addLayout(btnLayout);

  m_tabWidget->addTab(rawDataTab, tr("Raw Data"));
}

void SourceWindow::onGithubIssuesReceived(const QString &sourceId, const QJsonArray &issues) {
  if (sourceId != m_sourceId) {
    return;
  }

  QModelIndexList matches = m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  if (matches.isEmpty()) return;
  QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();

  QJsonArray compactIssues = rawData.value(QStringLiteral("local_githubIssues")).toArray();
  QHash<int, QJsonObject> merged;
  for (const QJsonValue &v : compactIssues) {
    QJsonObject issue = v.toObject();
    merged.insert(issue.value(QStringLiteral("number")).toInt(), issue);
  }

  for (const QJsonValue &v : issues) {
    QJsonObject issue = v.toObject();
    if (issue.contains(QStringLiteral("pull_request"))) {
      continue;
    }

    QJsonObject compactIssue;
    compactIssue[QStringLiteral("number")] = issue.value(QStringLiteral("number")).toInt();
    compactIssue[QStringLiteral("title")] = issue.value(QStringLiteral("title")).toString();
    compactIssue[QStringLiteral("state")] = issue.value(QStringLiteral("state")).toString();
    QJsonObject user = issue.value(QStringLiteral("user")).toObject();
    compactIssue[QStringLiteral("user")] = user.value(QStringLiteral("login")).toString();

    merged.insert(compactIssue.value(QStringLiteral("number")).toInt(), compactIssue);
  }

  QJsonArray updatedIssues;
  QList<int> keys = merged.keys();
  std::sort(keys.begin(), keys.end(), std::greater<int>());
  for (int key : keys) {
    updatedIssues.append(merged.value(key));
  }

  if (rawData.value(QStringLiteral("local_githubIssues")).toArray() != updatedIssues) {
    rawData[QStringLiteral("local_githubIssues")] = updatedIssues;
    m_sourceModel->updateSource(rawData);
  }

  updateGithubIssuesDisplay(updatedIssues);
}

void SourceWindow::updateGithubIssuesDisplay(const QJsonArray &issues) {
  m_issuesModel->removeRows(0, m_issuesModel->rowCount());

  QString selectedState = QStringLiteral("open");
  if (m_issuesStateCombo) {
    selectedState = m_issuesStateCombo->currentData().toString();
  }

  for (const QJsonValue &v : issues) {
    QJsonObject issue = v.toObject();

    QString issueState = issue.value(QStringLiteral("state")).toString();
    if (selectedState != QStringLiteral("all") && issueState != selectedState) {
      continue;
    }

    QList<QStandardItem *> row;
    QStandardItem *numItem = new QStandardItem();
    numItem->setData(issue.value(QStringLiteral("number")).toInt(), Qt::DisplayRole);
    numItem->setData(issue.value(QStringLiteral("number")).toInt(), Qt::UserRole);
    row.append(numItem);
    row.append(new QStandardItem(issue.value(QStringLiteral("title")).toString()));
    row.append(new QStandardItem(issueState));
    row.append(new QStandardItem(issue.value(QStringLiteral("user")).toString()));

    m_issuesModel->appendRow(row);
  }
}

void SourceWindow::onGithubIssueContextReceived(const QString &sourceId, int /*issueNumber*/, const QJsonObject &issue,
                                                const QJsonArray &comments) {
  if (sourceId != m_sourceId)
    return;

  if (m_createSessionFromIssueAction) {
    m_createSessionFromIssueAction->setText(tr("Create Session..."));
    m_createSessionFromIssueAction->setEnabled(m_issuesView && m_issuesView->selectionModel()->hasSelection());
  }
  if (m_cancelFetchAction) {
    m_cancelFetchAction->setEnabled(false);
  }
  m_fetchingIssueNumber = -1;

  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  QString sourceName = m_sourceId;
  QString owner = QStringLiteral("");
  QString repo = QStringLiteral("");
  if (!matches.isEmpty()) {
    sourceName = matches.first().data(SourceModel::NameRole).toString();
    QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();
    owner = SourceModel::githubOwner(rawData);
    repo = SourceModel::githubRepository(rawData);
  }

  QString prompt = generateGithubIssuePrompt(sourceName, owner, repo, issue, comments);

  QJsonObject initialData;
  initialData[QStringLiteral("prompt")] = prompt;
  Q_EMIT newSessionFromIssueRequested(m_sourceId, initialData);
}

void SourceWindow::onGithubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error) {
  if (sourceId != m_sourceId)
    return;

  if (m_createSessionFromIssueAction) {
    m_createSessionFromIssueAction->setText(tr("Create Session..."));
    m_createSessionFromIssueAction->setEnabled(m_issuesView && m_issuesView->selectionModel()->hasSelection());
  }
  if (m_cancelFetchAction) {
    m_cancelFetchAction->setEnabled(false);
  }
  m_fetchingIssueNumber = -1;

  if (error.type() == ApiError::Type::Canceled) {
    Q_EMIT statusMessage(tr("GitHub issue fetch cancelled"));
    return;
  }

  // Determine concise status message
  QString statusMsg;
  switch (error.type()) {
  case ApiError::Type::Authentication:
    statusMsg = tr("Authentication failed — check credentials");
    break;
  case ApiError::Type::PermissionDenied:
    statusMsg = tr("GitHub access denied — check repository/token permissions");
    break;
  case ApiError::Type::RateLimit:
    statusMsg = tr("GitHub rate limit reached — retry later"); // Or extract retry-after if available
    break;
  case ApiError::Type::NotFound:
    statusMsg = tr("GitHub issue #%1 could not be loaded").arg(issueNumber);
    break;
  case ApiError::Type::ServerError:
    statusMsg = tr("Service unavailable — try again later");
    break;
  default:
    statusMsg = tr("Failed to fetch issue context for #%1").arg(issueNumber);
    break;
  }

  Q_EMIT statusMessage(statusMsg);

  if (m_errorsModel) {
    QJsonObject errorObj = error.toJson();
    errorObj[QStringLiteral("sourceId")] = sourceId;
    errorObj[QStringLiteral("issueNumber")] = issueNumber;
    errorObj[QStringLiteral("operation")] = QStringLiteral("fetch GitHub issue context");
    errorObj[QStringLiteral("provider")] = QStringLiteral("GitHub");
    errorObj[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    // Remove the message overwrite if ApiError already has one, or use a rich one
    if (errorObj.value(QStringLiteral("message")).toString().isEmpty()) {
      errorObj[QStringLiteral("message")] =
          tr("Failed to fetch issue context for #%1: %2").arg(issueNumber).arg(error.message());
    }
    m_errorsModel->addErrorObj(errorObj);
  }
}

void SourceWindow::onGithubPullRequestsReceived(const QString &sourceId, const QJsonArray &prs) {
  if (sourceId != m_sourceId) {
    return;
  }

  m_prsModel->removeRows(0, m_prsModel->rowCount());

  for (const QJsonValue &v : prs) {
    QJsonObject pr = v.toObject();
    QList<QStandardItem *> row;

    QStandardItem *numItem = new QStandardItem();
    numItem->setData(pr.value(QStringLiteral("number")).toInt(), Qt::DisplayRole);
    row.append(numItem);

    QStandardItem *titleItem = new QStandardItem(pr.value(QStringLiteral("title")).toString());
    row.append(titleItem);

    QStandardItem *stateItem = new QStandardItem(pr.value(QStringLiteral("state")).toString());
    row.append(stateItem);

    QJsonObject user = pr.value(QStringLiteral("user")).toObject();
    QStandardItem *userItem = new QStandardItem(user.value(QStringLiteral("login")).toString());
    row.append(userItem);

    m_prsModel->appendRow(row);
  }

  m_prsView->resizeColumnToContents(0);
  m_prsView->resizeColumnToContents(2);
  m_prsView->resizeColumnToContents(3);

  QModelIndexList matches =
      m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::IdRole, m_sourceId, 1, Qt::MatchExactly);
  if (!matches.isEmpty()) {
    QJsonObject rawData = matches.first().data(SourceModel::RawDataRole).toJsonObject();

    QJsonArray compactPRs;
    for (const QJsonValue &v : prs) {
      QJsonObject pr = v.toObject();
      QJsonObject compactPR;
      compactPR[QStringLiteral("number")] = pr.value(QStringLiteral("number"));
      compactPR[QStringLiteral("title")] = pr.value(QStringLiteral("title"));
      compactPR[QStringLiteral("state")] = pr.value(QStringLiteral("state"));
      compactPR[QStringLiteral("html_url")] = pr.value(QStringLiteral("html_url"));

      QJsonObject userObj;
      userObj[QStringLiteral("login")] = pr.value(QStringLiteral("user")).toObject().value(QStringLiteral("login"));
      compactPR[QStringLiteral("user")] = userObj;

      compactPRs.append(compactPR);
    }

    if (rawData.value(QStringLiteral("local_githubPRs")).toArray() != compactPRs) {
      rawData[QStringLiteral("local_githubPRs")] = compactPRs;
      m_sourceModel->updateSourceRaw(m_sourceId, rawData);

      if (m_rawDataEdit) {
        QJsonDocument doc(rawData);
        m_rawDataEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
      }
    }
  }
}

#include "sourcewindow.moc"
