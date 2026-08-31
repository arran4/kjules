#include <functional>
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "api/apierror.h"
#include <KXmlGuiWindow>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMultiMap>
#include <QSystemTrayIcon>

#include "clickablelabel.h"
#include "queuescheduler.h"
#include "sessionswindow.h"

class APIManager;
class SessionModel;
class SourceModel;
class QTextBrowser;
class SessionWindow;
class NewSessionDialog;
class DraftsModel;
class TemplatesModel;
class QueueModel;
struct QueueItem;
class ErrorsModel;
class BlockedTreeModel;
class QAbstractItemView;
class QAbstractItemModel;
class QSortFilterProxyModel;

class QListView;
class QTreeView;
class FilterEditor;
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QAction;
class QVBoxLayout;
class RefreshProgressWindow;
class SourcesRefreshProgressWindow;
class ClickableProgressBar;
struct SourceRemapEntry;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT
  friend class TestSourceWindow;

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  void setMockApi(bool useMock);

Q_SIGNALS:
  void sessionAutoArchived(const QString &id, const QString &reason);
  void statusMessage(const QString &message);

protected:
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private Q_SLOTS:
  void deleteFollowingSessions();
  void snoozeSelectedFollowingSessions(const QDateTime &until);
  void snoozeSelectedFollowingSessionsCustom();
  void clearSnoozeSelectedFollowingSessions();
  void archiveSelectedSessions();
  void deleteArchiveSessions();
  void deleteDrafts();
  void deleteTemplates();
  void deleteErrors();
  void processSessionModel(SessionModel *model, int &sessionCount);

  void switchToFollowingTab();
  void onSessionReloaded(const QJsonObject &session, bool isBackground);
  void addGithubLink(QMenu *githubMenu, const QString &urlStr, const QString &title, const QString &path);

  void updateCompletions();
  void refreshSources();
  void refreshSourcesImpl(bool isBackground);
  void refreshGithubDataForSources(const QStringList &sourceIds);
public Q_SLOTS:
  void showNewSessionDialogSlot();
private Q_SLOTS:
  NewSessionDialog *showNewSessionDialog(const QJsonObject &initialData = QJsonObject(), bool ignoreSelection = false);
  void showCreateRepoDialog();
  void showSourceStatusDialog(const QString &sourceName);
  void openSourceWindow(const QString &sourceId);
  void showManageCustomSourcesDialog();
  void showSettingsDialog();
  void onSessionCreated(const QMultiMap<QString, QString> &sources, const QString &prompt,
                        const QString &automationMode, bool requirePlanApproval, bool ignoreConcurrency,
                        int priority = 0, const QString &queueAction = QString());
  void onCreateRepoAndSession(const QString &org, const QString &repoName, bool isPrivate, const QString &prompt,
                              const QString &automationMode, bool requirePlanApproval, bool ignoreConcurrency);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
  void onTemplateSaved(const QJsonObject &tmpl);
  void onTemplateActivated(const QModelIndex &index);
  void onQueueActivated(const QModelIndex &index);
  void onQueueContextMenu(const QPoint &pos);
  void onHoldingActivated(const QModelIndex &index);
  void onHoldingContextMenu(const QPoint &pos);
  void onBlockedContextMenu(const QPoint &pos);
  void onErrorActivated(const QModelIndex &index);
  void onSessionCreationFailed(const QJsonObject &request, const ApiError &apiError, const QString &httpDetails);
  void onSessionActivated(const QModelIndex &index);
  void onSourceActivated(const QModelIndex &index);
  void showSessionWindow(const QJsonObject &session);
  void connectSessionWindow(SessionWindow *window);
  void connectNewSessionDialog(NewSessionDialog *window);
  void updateStatus(const QString &message);
  void onError(const QString &message);
  void toggleWindow();
  void toggleWindowVisibility();
  void onSourcesReceived(const QJsonArray &sources);
  void onSourcesRefreshFinished(bool complete);
  void onUnseenErrorsCountChanged(int count);
  void onGithubInfoReceived(const QString &sourceId, const QJsonObject &info);
  void onGithubBranchesReceived(const QString &sourceId, const QJsonArray &branches);
  void onGithubPullRequestInfoReceived(const QString &prUrl, const QJsonObject &info);
  void cancelSourcesRefresh();
  void updateSessionStats();
  void updateTrayToolTip();
  void onSourceDetailsReceived(const QJsonObject &source);
  void toggleFavourite();
  void increaseFavouriteRank();
  void decreaseFavouriteRank();
  void setFavouriteRank();
  bool processQueue();
  void scheduleNextQueueAttempt();
  void onMasterMinuteTimer();
  void onMasterSecondTimer();
  void refreshBeforeQueue();
  void checkPendingRefreshBeforeQueue(const QString &id);
  QStringList getActiveFollowingSessionIds() const;
  void updateHoldingTabVisibility();
  void updateBlockedTabVisibility();

  void onSessionCreatedResult(bool success, const QJsonObject &session, const ApiError &apiError = ApiError(),
                              const QString &rawResponse = QString());
  void onGithubRepoCreatedResult(bool success, const QJsonObject &requestData, const QJsonObject &response,
                                 const ApiError &apiError = ApiError());
  void sendQueueItemNow(int row);
  void sendItemNow(const QueueItem &item, int originRow, bool sourceIsQueue,
                   const QJsonObject &errData = QJsonObject());
  void editQueueItem(int row);
  void convertQueueItemToDraft(int row);
  void showErrorDetails(int row, QueueModel *model);
  void requeueError(int sourceRow);
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void backupData();
  void restoreData();
  void exportTemplates();
  void importTemplates();
  void copyTemplateToClipboard(const QModelIndex &index);
  void pasteTemplateFromClipboard();
  void duplicateFollowingItemsToQueue(const QString &targetState, const QString &stateName);
  void toggleQueueState();
  void loadQueueSettings();
  void updateTabTitles();
  void updateSelectionDependentActions();
  void connectModelForTabUpdates(QAbstractItemModel *model);
  bool resolvePendingGithubSource();
  void checkAutoArchiveSessions();
  void updateCountdownStatus();
  void onRefreshProgressUpdated(int current, int total);
  void onRefreshProgressFinished();
  void onSessionRefreshProgressBarClicked();
  void showFixSourcesDialog();

private Q_SLOTS:
  void autoRefreshFollowing();
  void updateFavouritesMenu();

public:
  SessionModel *sessionModel() const { return m_sessionModel; }
  SessionModel *archiveModel() const { return m_archiveModel; }
  APIManager *apiManager() const { return m_apiManager; }

private:
  QList<int> getUniqueSortedRows(const QList<QModelIndex> &selectedRows, const QAbstractItemView *view) const;
  void applyFavouriteAction(
      std::function<void(const QSortFilterProxyModel *, QAbstractItemModel *, const QList<QModelIndex> &, int)> action);

  QStringList getSelectedSessionIds() const;
  QString urlFromSource(const QJsonObject &source) const;

  void setupUi();

  void setupSourcesTab(QWidget *tab);
  void setupFollowingTab(QWidget *tab);
  void setupSnoozedTab(QWidget *tab);
  void setupArchiveTab(QWidget *tab);
  void setupDraftsTab(QWidget *tab);
  void setupTemplatesTab(QWidget *tab);
  void setupQueueTab();
  void setupHoldingTab();
  void mergeLegacyData();
  void setupBlockedTab();
  void setupErrorsTab(QWidget *tab);
  void setupStatusBar();
  void setupTrayIcon();
  void createActions();
  void createGeneralActions();
  void createSessionActions();
  void createSourceActions();
  void setupSourceSettingsAction();
  void setupRefreshSourceActions();
  void setupRecalculateStatsAction();
  void setupShowFollowingNewSessionsAction();
  void setupViewRawDataAction();
  void setupUrlActions();
  void createDataActions();
  void createQueueActions();
  void createArchiveActions();
  void createFilterActions();
  void createRefreshActions();
  void createStandardActions();
  void connectSignals();
  void showFixSourcesDialog(const QString &onlySource);
  QList<SourceRemapEntry> pendingSourceEntries(const QString &onlySource = QString()) const;
  void applySourceRemaps(const QList<SourceRemapEntry> &entries, const QStringList &newSources);

  APIManager *m_apiManager;
  QHash<QString, QString> m_previousSessionStates;
  QHash<QString, QString> m_previousSessionPrStates;
  SessionModel *m_sessionModel;
  SessionModel *m_archiveModel;
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;
  TemplatesModel *m_templatesModel;
  QueueModel *m_queueModel;
  QueueModel *m_holdingModel;
  ErrorsModel *m_errorsModel;

  QTreeView *m_sourceView;
  QTreeView *m_sessionView;
  QTreeView *m_snoozedView;
  QTreeView *m_archiveView;
  QListView *m_draftsView;
  QListView *m_templatesView;
  QListView *m_queueView;
  QListView *m_holdingView;
  QListView *m_errorsView;
  QTreeView *m_blockedView;
  BlockedTreeModel *m_blockedTreeModel;
  std::function<void()> m_deleteQueueItemsLambda;
  std::function<void()> m_deleteHoldingItemsLambda;
  QMenu *m_favouritesMenu = nullptr;
  FilterEditor *m_sourcesFilterEditor;
  FilterEditor *m_followingFilterEditor;
  FilterEditor *m_snoozedFilterEditor;
  FilterEditor *m_archiveFilterEditor;
  QLineEdit *m_draftsFilter;
  QLineEdit *m_templatesFilter;
  QLineEdit *m_errorsFilter;
  QTabWidget *m_tabWidget;
  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  ClickableLabel *m_statusLabel;
  ClickableLabel *m_unseenErrorLabel;
  QLabel *m_sessionStatsLabel;
  QLabel *m_queueCountdownLabel;
  ClickableProgressBar *m_sourceProgressBar;
  SourcesRefreshProgressWindow *m_sourcesRefreshProgressWindow;
  ClickableProgressBar *m_sessionRefreshProgressBar;
  QPushButton *m_cancelRefreshBtn;
  QAction *m_refreshSourcesAction;
  QAction *m_refreshFollowingAction;
  QAction *m_refreshSourceAction;

  QAction *m_refreshCurrentTabAction;
  QAction *m_refreshInProgressAction;
  QAction *m_refreshCompleteAction;
  QAction *m_refreshWaitingFeedbackAction;
  QAction *m_refreshFollowingGithubAction;
  QAction *m_refreshSourcesAllAction;
  QAction *m_refreshSourcesGithubAction;
  QAction *m_recalculateStatsAction;
  QAction *m_showFullSessionListAction;
  QAction *m_followFromIdAction;
  QAction *m_toggleFavouriteAction;
  QAction *m_viewSessionsAction;
  QAction *m_showFollowingNewSessionsAction;
  QAction *m_viewRawDataAction;
  QAction *m_sourceSettingsAction;
  QAction *m_openUrlAction;
  QAction *m_openCurrentUrlAction;
  QAction *m_openAllGithubUrlsAction;
  QAction *m_openAllGithubInProgressAction;
  QAction *m_openAllGithubCompleteAction;
  QAction *m_openAllGithubWaitingFeedbackAction;
  QAction *m_openAllJulesUrlsAction;
  QAction *m_openAllJulesInProgressAction;
  QAction *m_openAllJulesCompleteAction;
  QAction *m_openAllJulesWaitingFeedbackAction;
  QAction *m_openAllJulesNoGithubUrlsAction;
  QAction *m_openAllJulesNoGithubInProgressAction;
  QAction *m_openAllJulesNoGithubCompleteAction;
  QAction *m_openAllJulesNoGithubWaitingFeedbackAction;
  QAction *m_manageCustomSourcesAction;
  QAction *m_copyUrlAction;
  QAction *m_showActivityLogAction;
  QAction *m_backupDataAction;
  QAction *m_restoreDataAction;
  QAction *m_importTemplatesAction;
  QAction *m_exportTemplatesAction;
  QAction *m_toggleQueueAction;
  QAction *m_archiveMergedFollowingAction;
  QAction *m_archivePausedFollowingAction;
  QAction *m_archiveFailedFollowingAction;
  QAction *m_archiveCompletedFollowingAction;
  QAction *m_archiveCanceledFollowingAction;
  QAction *m_duplicatePausedToQueueAndArchiveAction;
  QAction *m_duplicateCanceledToQueueAndArchiveAction;
  QAction *m_duplicateFailedToQueueAndArchiveAction;
  QAction *m_purgeArchiveAction;
  QAction *m_openJulesUrlAction;
  QAction *m_openJulesUrlsAwaitingFeedbackAction;
  QAction *m_openJulesUrlsCompletedNoPrAction;
  QAction *m_openJulesUrlsCompletedNoPrOrFeedbackAction;
  QAction *m_openGithubUrlAction;
  QAction *m_configureConcurrencyLimitAction;
  QAction *m_viewFilterArchivedAction;
  QAction *m_viewFilterForksAction;
  QAction *m_viewFilterPrivateAction;
  QAction *m_createRepoAndSessionAction;
  QAction *m_fixSourcesAction;
  QAction *m_mergeLegacyDataAction;

  bool m_isRefreshingSources;
  int m_sourcesLoadedCount;
  int m_sourcesAddedCount;
  int m_pagesLoadedCount;
  QJsonArray m_refreshedSources;
  QDateTime m_lastSessionRefreshTime;
  QString m_lastStatusMessage;
  QTimer *m_masterMinuteTimer;
  QTimer *m_masterSecondTimer;
  bool m_isProcessingQueue;
  bool m_isProcessingMinuteTimer;
  QueueScheduler m_queueScheduler;
  bool m_queuePaused;
  QSet<QString> m_pendingRefreshIds;
  QSet<QString> m_inFlightSessionReloads;
  QHash<QString, QDateTime> m_sessionReloadFailedAt;
  bool m_isWaitingForRefreshBeforeQueue;
  bool m_isWaitingForCreatedRepoSource = false;

  RefreshProgressWindow *m_refreshProgressWindow;
};

#endif // MAINWINDOW_H
