#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QDateTime>
#include <QSystemTrayIcon>

#include "sessionswindow.h"

class APIManager;
class SessionModel;
class SourceModel;
class QTextBrowser;
class SessionWindow;
class DraftsModel;
class TemplatesModel;
class QueueModel;
class ErrorsModel;
class QListView;
class QTreeView;
class FilterEditor;
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QAction;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  void setMockApi(bool useMock);

Q_SIGNALS:
  void sessionAutoArchived(const QString &id, const QString &reason);

protected:
  void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
  void updateCompletions();
  void refreshSources();
  void showNewSessionDialog();
  void showSettingsDialog();
  void onSessionCreated(const QMap<QString, QString> &sources,
                        const QString &prompt, const QString &automationMode,
                        bool requirePlanApproval);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
  void onTemplateSaved(const QJsonObject &tmpl);
  void onTemplateActivated(const QModelIndex &index);
  void onQueueActivated(const QModelIndex &index);
  void onQueueContextMenu(const QPoint &pos);
  void onErrorActivated(const QModelIndex &index);
  void onSessionCreationFailed(const QJsonObject &request,
                               const QJsonObject &response,
                               const QString &errorString,
                               const QString &httpDetails);
  void onSessionActivated(const QModelIndex &index);
  void onSourceActivated(const QModelIndex &index);
  void showSessionWindow(const QJsonObject &session);
  void connectSessionWindow(SessionWindow *window);
  void updateStatus(const QString &message);
  void onError(const QString &message);
  void toggleWindow();
  void toggleWindowVisibility();
  void onSourcesReceived(const QJsonArray &sources);
  void onSourcesRefreshFinished();
  void onGithubInfoReceived(const QString &sourceId, const QJsonObject &info);
  void onGithubPullRequestInfoReceived(const QString &prUrl,
                                       const QJsonObject &info);
  void cancelSourcesRefresh();
  void updateSessionStats();
  void onSourceDetailsReceived(const QJsonObject &source);
  void toggleFavourite();
  void processQueue();
  void processErrorRetries();
  void onSessionCreatedResult(bool success, const QJsonObject &session,
                              const QString &errorMsg,
                              const QString &rawResponse = QString());
  void sendQueueItemNow(int row);
  void editQueueItem(int row);
  void convertQueueItemToDraft(int row);
  void showErrorDetails(int row);
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void backupData();
  void restoreData();
  void exportTemplates();
  void importTemplates();
  void copyTemplateToClipboard(const QModelIndex &index);
  void pasteTemplateFromClipboard();
  void toggleQueueState();
  void loadQueueSettings();
  void updateTabTitles();
  void connectModelForTabUpdates(QAbstractItemModel *model);
  void checkAutoArchiveSessions();
  void updateCountdownStatus();

private:
  void setupUi();
  void setupTrayIcon();
  void createActions();

  APIManager *m_apiManager;
  QHash<QString, QString> m_previousSessionStates;
  QHash<QString, QString> m_previousSessionPrStates;
  SessionModel *m_sessionModel;
  SessionModel *m_archiveModel;
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;
  TemplatesModel *m_templatesModel;
  QueueModel *m_queueModel;
  ErrorsModel *m_errorsModel;
  QTimer *m_errorRetryTimer;

  QTreeView *m_sourceView;
  QTreeView *m_sessionView;
  QTreeView *m_archiveView;
  QListView *m_draftsView;
  QListView *m_templatesView;
  QListView *m_queueView;
  QListView *m_errorsView;
  std::function<void()> m_deleteQueueItemsLambda;
  FilterEditor *m_sourcesFilterEditor;
  FilterEditor *m_followingFilterEditor;
  FilterEditor *m_archiveFilterEditor;
  QLineEdit *m_draftsFilter;
  QLineEdit *m_templatesFilter;
  QLineEdit *m_errorsFilter;
  QTabWidget *m_tabWidget;
  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  QLabel *m_statusLabel;
  QLabel *m_sessionStatsLabel;
  QProgressBar *m_sourceProgressBar;
  QPushButton *m_cancelRefreshBtn;
  QAction *m_refreshSourcesAction;
  QAction *m_refreshFollowingAction;
  QAction *m_refreshSourceAction;
  QAction *m_recalculateStatsAction;
  QAction *m_showFullSessionListAction;
  QAction *m_followFromIdAction;
  QAction *m_toggleFavouriteAction;
  QAction *m_viewSessionsAction;
  QAction *m_showFollowingNewSessionsAction;
  QAction *m_viewRawDataAction;
  QAction *m_openUrlAction;
  QAction *m_copyUrlAction;
  QAction *m_showActivityLogAction;
  QAction *m_backupDataAction;
  QAction *m_restoreDataAction;
  QAction *m_importTemplatesAction;
  QAction *m_exportTemplatesAction;
  QAction *m_toggleQueueAction;

  bool m_isRefreshingSources;
  int m_sourcesLoadedCount;
  int m_sourcesAddedCount;
  int m_pagesLoadedCount;
  QTimer *m_sessionRefreshTimer;
  QDateTime m_lastSessionRefreshTime;

  QTimer *m_queueTimer;
  QTimer *m_countdownTimer;
  bool m_isProcessingQueue;
  QDateTime m_queueBackoffUntil;
  bool m_queuePaused;
};

#endif // MAINWINDOW_H
