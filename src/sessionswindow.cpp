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
#include <QTabWidget>
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

  m_sessionsWidget = new SessionsWidget(filterSource, apiManager, managedModel, this);
  setCentralWidget(m_sessionsWidget);

  connect(m_sessionsWidget, &SessionsWidget::watchRequested, this, &SessionsWindow::watchRequested);
  connect(m_sessionsWidget, &SessionsWidget::archiveRequested, this, &SessionsWindow::archiveRequested);
  connect(m_sessionsWidget, &SessionsWidget::deleteRequested, this, &SessionsWindow::deleteRequested);

  setupActions();

  setupGUI(Default, QStringLiteral(":/kxmlgui6/org.kde.kjules/sessionswindowui.rc"));
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

  QAction *focusFilterAction = new QAction(i18n("Focus Filter"), this);
  actionCollection()->addAction(QStringLiteral("focus_filter"), focusFilterAction);
  actionCollection()->setDefaultShortcut(focusFilterAction, QKeySequence(Qt::CTRL | Qt::Key_F));
  connect(focusFilterAction, &QAction::triggered, m_sessionsWidget, &SessionsWidget::focusFilter);

  m_watchMenuAction = new QAction(i18n("Watch Session"), this);
  actionCollection()->addAction(QStringLiteral("watch_session"), m_watchMenuAction);
  m_watchMenuAction->setEnabled(false);

  m_archiveMenuAction = new QAction(i18n("Archive Session"), this);
  actionCollection()->addAction(QStringLiteral("archive_session"), m_archiveMenuAction);
  m_archiveMenuAction->setEnabled(false);

  m_deleteMenuAction = new QAction(i18n("Unmanage Session"), this);
  actionCollection()->addAction(QStringLiteral("delete_session"), m_deleteMenuAction);
  m_deleteMenuAction->setEnabled(false);

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
  autoLoadBottomAction->setData(QStringLiteral("load_bottom"));
  m_autoLoadGroup->addAction(autoLoadBottomAction);
  actionCollection()->addAction(QStringLiteral("auto_load_bottom"), autoLoadBottomAction);

  m_autoFollowAction = new QAction(i18n("Automatically Follow Active Sessions"), this);
  m_autoFollowAction->setCheckable(true);
  actionCollection()->addAction(QStringLiteral("auto_follow_refresh"), m_autoFollowAction);

  m_sessionsWidget->setAutoLoadBehavior(m_autoLoadGroup);

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
  QString autoLoadBehavior = config.readEntry("AutoLoadBehavior", QStringLiteral("manual"));
  if (autoLoadBehavior == QStringLiteral("load_all")) {
    autoLoadAllAction->setChecked(true);
  } else if (autoLoadBehavior == QStringLiteral("load_bottom")) {
    autoLoadBottomAction->setChecked(true);
  } else {
    autoLoadManualAction->setChecked(true);
  }

  bool autoFollow = config.readEntry("AutoFollowActive", false);
  m_autoFollowAction->setChecked(autoFollow);
  m_sessionsWidget->setAutoFollowOnRefresh(autoFollow);

  connect(m_autoLoadGroup, &QActionGroup::triggered, this, [this](QAction *action) {
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
    config.writeEntry("AutoLoadBehavior", action->data().toString());
  });

  connect(m_autoFollowAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
    config.writeEntry("AutoFollowActive", checked);
    m_sessionsWidget->setAutoFollowOnRefresh(checked);
  });
}
