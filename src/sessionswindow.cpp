#include "sessionswindow.h"
#include "apimanager.h"
#include "sessionmodel.h"
#include "sessionswidget.h"
#include "utils.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

SessionsWindow::SessionsWindow(const QString &filterSource, APIManager *apiManager, SessionModel *managedModel,
                               QWidget *parent)
    : KXmlGuiWindow(parent) {
  setObjectName(QStringLiteral("SessionsWindow"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(800, 600);

  if (!filterSource.isEmpty()) {
    setWindowTitle(i18n("Sessions for %1", filterSource));
  } else {
    setWindowTitle(i18n("All Sessions"));
  }

  m_sessionsWidget = new SessionsWidget(filterSource, apiManager, managedModel, nullptr, this);
  setCentralWidget(m_sessionsWidget);

  connect(m_sessionsWidget, &SessionsWidget::watchRequested, this, &SessionsWindow::watchRequested);
  connect(m_sessionsWidget, &SessionsWidget::archiveRequested, this, &SessionsWindow::archiveRequested);
  connect(m_sessionsWidget, &SessionsWidget::deleteRequested, this, &SessionsWindow::deleteRequested);

  setupActions();

  setupGUI(ToolBar | Keys | StatusBar | Create, QStringLiteral(KJULES_KXMLGUI_RESOURCE_PREFIX "sessionswindowui.rc"));
}

SessionsWindow::~SessionsWindow() {}

void SessionsWindow::setupActions() {
  QAction *refreshAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
  connect(refreshAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::refreshSessions);
  actionCollection()->addAction(QStringLiteral("refresh_sessions"), refreshAction);
  actionCollection()->setDefaultShortcut(refreshAction, QKeySequence(Qt::Key_F5));

  m_resumeAction = new QAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("Load More"), this);
  connect(m_resumeAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::resumeRefresh);
  actionCollection()->addAction(QStringLiteral("resume_refresh"), m_resumeAction);
  m_resumeAction->setEnabled(false);

  connect(m_sessionsWidget, &SessionsWidget::canResumeChanged, m_resumeAction, &QAction::setEnabled);

  m_loadRemainingAction = new QAction(QIcon::fromTheme(QStringLiteral("go-bottom")), i18n("Load Remaining"), this);
  connect(m_loadRemainingAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::loadRemainingRefresh);
  actionCollection()->addAction(QStringLiteral("load_remaining"), m_loadRemainingAction);
  m_loadRemainingAction->setEnabled(false);

  connect(m_sessionsWidget, &SessionsWidget::canResumeChanged, m_loadRemainingAction, &QAction::setEnabled);

  QAction *closeAction = new QAction(i18n("Close"), this);
  actionCollection()->addAction(QStringLiteral("file_close"), closeAction);
  connect(closeAction, &QAction::triggered, this, &SessionsWindow::close);
  actionCollection()->setDefaultShortcut(closeAction, QKeySequence(Qt::CTRL | Qt::Key_W));

  QAction *focusFilterAction = new QAction(i18n("Focus Filter"), this);
  actionCollection()->addAction(QStringLiteral("focus_filter"), focusFilterAction);
  actionCollection()->setDefaultShortcut(focusFilterAction, QKeySequence(Qt::CTRL | Qt::Key_F));
  connect(focusFilterAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::focusFilter);

  m_watchMenuAction = new QAction(i18n("Watch Session"), this);
  actionCollection()->addAction(QStringLiteral("watch_session"), m_watchMenuAction);
  m_watchMenuAction->setEnabled(false);
  connect(m_watchMenuAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::watchSelectedSessions);

  m_archiveMenuAction = new QAction(i18n("Archive Session"), this);
  actionCollection()->addAction(QStringLiteral("archive_session"), m_archiveMenuAction);
  m_archiveMenuAction->setEnabled(false);
  connect(m_archiveMenuAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::archiveSelectedSessions);

  m_deleteMenuAction = new QAction(i18n("Unmanage Session"), this);
  actionCollection()->addAction(QStringLiteral("delete_session"), m_deleteMenuAction);
  m_deleteMenuAction->setEnabled(false);
  connect(m_deleteMenuAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::unmanageSelectedSessions);

  connect(m_sessionsWidget, &SessionsWidget::actionStatesChanged, this,
          [this](bool canWatch, bool canArchive, bool canDelete) {
            m_watchMenuAction->setEnabled(canWatch);
            m_archiveMenuAction->setEnabled(canArchive);
            m_deleteMenuAction->setEnabled(canDelete);
          });

  m_autoLoadGroup = new QActionGroup(this);

  QAction *autoLoadManualAction = new QAction(i18n("Manual"), this);
  autoLoadManualAction->setCheckable(true);
  autoLoadManualAction->setData(QStringLiteral("manual"));
  m_autoLoadGroup->addAction(autoLoadManualAction);
  actionCollection()->addAction(QStringLiteral("auto_load_manual"), autoLoadManualAction);

  QAction *autoLoadAllAction = new QAction(i18n("Load All Automatically"), this);
  autoLoadAllAction->setCheckable(true);
  autoLoadAllAction->setData(QStringLiteral("load_all"));
  m_autoLoadGroup->addAction(autoLoadAllAction);
  actionCollection()->addAction(QStringLiteral("auto_load_all"), autoLoadAllAction);

  QAction *autoLoadBottomAction = new QAction(i18n("Load When Scrolling to Bottom"), this);
  autoLoadBottomAction->setCheckable(true);
  autoLoadBottomAction->setData(QStringLiteral("auto_bottom"));
  m_autoLoadGroup->addAction(autoLoadBottomAction);
  actionCollection()->addAction(QStringLiteral("auto_load_bottom"), autoLoadBottomAction);

  m_autoFollowAction = new QAction(i18n("Automatically Follow Active Sessions"), this);
  m_autoFollowAction->setCheckable(true);
  actionCollection()->addAction(QStringLiteral("auto_follow_refresh"), m_autoFollowAction);

  m_sessionsWidget->setAutoLoadBehavior(m_autoLoadGroup);

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
  QString autoLoadMode = config.readEntry("AutoLoadMode", QString());
  if (autoLoadMode.isEmpty()) {
    QString migrated = config.readEntry("AutoLoadBehavior", QStringLiteral("manual"));
    if (migrated == QStringLiteral("load_bottom")) {
      autoLoadMode = QStringLiteral("auto_bottom");
    } else {
      autoLoadMode = migrated;
    }
  }

  if (autoLoadMode == QStringLiteral("load_all")) {
    autoLoadAllAction->setChecked(true);
  } else if (autoLoadMode == QStringLiteral("auto_bottom")) {
    autoLoadBottomAction->setChecked(true);
  } else {
    autoLoadManualAction->setChecked(true);
  }

  bool autoFollow = config.readEntry("AutoFollowRefresh", false);
  if (!config.hasKey("AutoFollowRefresh") && config.hasKey("AutoFollowActive")) {
    autoFollow = config.readEntry("AutoFollowActive", false);
  }
  m_autoFollowAction->setChecked(autoFollow);
  m_sessionsWidget->setAutoFollowOnRefresh(autoFollow);

  connect(m_autoLoadGroup, &QActionGroup::triggered, this, [this](QAction *action) {
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
    config.writeEntry("AutoLoadMode", action->data().toString());
    config.sync();
  });

  connect(m_autoFollowAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
    config.writeEntry("AutoFollowRefresh", checked);
    config.sync();
    m_sessionsWidget->setAutoFollowOnRefresh(checked);
  });

  auto addColumnToggle = [this, &config](const QString &label, int colIndex, const QString &actionName) {
    QAction *action = new QAction(label, this);
    action->setCheckable(true);

    QString key = QStringLiteral("ShowColumn_%1").arg(colIndex);
    bool isVisible = config.readEntry(key, true);
    action->setChecked(isVisible);
    m_sessionsWidget->listView()->header()->setSectionHidden(colIndex, !isVisible);

    connect(action, &QAction::toggled, [this, colIndex](bool checked) {
      m_sessionsWidget->listView()->header()->setSectionHidden(colIndex, !checked);
      KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
      config.writeEntry(QStringLiteral("ShowColumn_%1").arg(colIndex), checked);
      config.sync();
    });

    actionCollection()->addAction(actionName, action);
  };

  addColumnToggle(i18n("Title"), SessionModel::ColTitle, QStringLiteral("col_title"));
  addColumnToggle(i18n("State"), SessionModel::ColState, QStringLiteral("col_state"));
  addColumnToggle(i18n("Change Set"), SessionModel::ColChangeSet, QStringLiteral("col_changeset"));
  addColumnToggle(i18n("PR"), SessionModel::ColPR, QStringLiteral("col_pr"));
  addColumnToggle(i18n("Updated At"), SessionModel::ColUpdatedAt, QStringLiteral("col_updatedat"));
  addColumnToggle(i18n("Created At"), SessionModel::ColCreatedAt, QStringLiteral("col_createdat"));
  addColumnToggle(i18n("Owner"), SessionModel::ColOwner, QStringLiteral("col_owner"));
  addColumnToggle(i18n("Repo"), SessionModel::ColRepo, QStringLiteral("col_repo"));
  addColumnToggle(i18n("ID"), SessionModel::ColId, QStringLiteral("col_id"));
}
