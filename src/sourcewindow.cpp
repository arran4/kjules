#include "sourcewindow.h"
#include "blockedtreemodel.h"
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

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
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
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeView>
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

void SourceWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);
  layout->addWidget(m_tabWidget);

  setupFollowingTab();
  setupArchivedTab();
  setupQueuedBlockedTab();
  setupSessionsTab();
  setupSettingsTab();
  setupRawDataTab();

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
    SessionWindow *win = new SessionWindow(session, m_apiManager, true, this);
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
    SessionWindow *win = new SessionWindow(session, m_apiManager, false, this);
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
  m_sessionsWidget = new SessionsWidget(m_sourceId, m_apiManager, m_sessionModel, this);
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

#include "sourcewindow.moc"
