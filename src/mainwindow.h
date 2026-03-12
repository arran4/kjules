#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QDateTime>
#include <QSystemTrayIcon>

#include "sessionswindow.h"

class APIManager;
class SessionModel;
class SourceModel;
class DraftsModel;
class QueueModel;
class ErrorsModel;
class QListView;
class QTreeView;
class QLabel;
class QProgressBar;
class QPushButton;
class QAction;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
  void refreshSources();
  void showNewSessionDialog();
  void showSettingsDialog();
  void onSessionCreated(const QStringList &sources, const QString &prompt,
                        const QString &automationMode,
                        bool requirePlanApproval);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
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
  void updateStatus(const QString &message);
  void onError(const QString &message);
  void toggleWindow();
  void toggleWindowVisibility();
  void onSourcesReceived(const QJsonArray &sources);
  void onSourcesRefreshFinished();
  void cancelSourcesRefresh();
  void updateSessionStats();
  void onSourceDetailsReceived(const QJsonObject &source);
  void processQueue();
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
  void toggleQueueState();
  void loadQueueSettings();

#ifdef MOCK_UI_TEST
public:
  void loadMockData();
#endif

private:
  void setupUi();
  void setupTrayIcon();
  void createActions();

  APIManager *m_apiManager;
  SessionModel *m_sessionModel;
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;
  QueueModel *m_queueModel;
  ErrorsModel *m_errorsModel;

  QTreeView *m_sourceView;
  QListView *m_sessionView;
  QListView *m_draftsView;
  QListView *m_queueView;
  QListView *m_errorsView;
  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  QLabel *m_statusLabel;
  QLabel *m_sessionStatsLabel;
  QProgressBar *m_sourceProgressBar;
  QPushButton *m_cancelRefreshBtn;
  QAction *m_refreshSourcesAction;
  QAction *m_refreshSourceAction;
  QAction *m_showFullSessionListAction;
  QAction *m_viewSessionsAction;
  QAction *m_showPastNewSessionsAction;
  QAction *m_viewRawDataAction;
  QAction *m_openUrlAction;
  QAction *m_copyUrlAction;
  QAction *m_backupDataAction;
  QAction *m_restoreDataAction;
  QAction *m_toggleQueueAction;

  bool m_isRefreshingSources;
  int m_sourcesLoadedCount;
  int m_sourcesAddedCount;
  int m_pagesLoadedCount;
  QTimer *m_sessionRefreshTimer;
  QDateTime m_lastSessionRefreshTime;

  QTimer *m_queueTimer;
  bool m_isProcessingQueue;
  QDateTime m_queueBackoffUntil;
  bool m_queuePaused;
};

#endif // MAINWINDOW_H
