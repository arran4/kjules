#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QDateTime>
#include <QSystemTrayIcon>

class APIManager;
class SessionModel;
class SourceModel;
class DraftsModel;
class QueueModel;
class QListView;
class KStatusNotifierItem;
class QLabel;
class QProgressBar;
class QPushButton;
class QAction;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private Q_SLOTS:
  void refreshSources();
  void refreshSessions();
  void showNewSessionDialog();
  void showSettingsDialog();
  void onSessionCreated(const QStringList &sources, const QString &prompt,
                        const QString &automationMode);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
  void onQueueActivated(const QModelIndex &index);
  void onQueueContextMenu(const QPoint &pos);
  void onSessionActivated(const QModelIndex &index);
  void onSourceActivated(const QModelIndex &index);
  void showSessionWindow(const QJsonObject &session);
  void updateStatus(const QString &message);
  void onError(const QString &message);
  void toggleWindow();
  void onSourcesReceived(const QJsonArray &sources);
  void onSourcesRefreshFinished();
  void cancelSourcesRefresh();
  void updateSessionStats();
  void processQueue();
  void onSessionCreatedResult(bool success, const QJsonObject &session,
                              const QString &errorMsg);
  void sendQueueItemNow(int row);
  void editQueueItem(int row);
  void convertQueueItemToDraft(int row);

private:
  void setupUi();
  void setupTrayIcon();
  void createActions();

  APIManager *m_apiManager;
  SessionModel *m_sessionModel;
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;
  QueueModel *m_queueModel;

  QListView *m_sourceView;
  QListView *m_sessionView;
  QListView *m_draftsView;
  QListView *m_queueView;
  KStatusNotifierItem *m_trayIcon;
  QLabel *m_statusLabel;
  QLabel *m_sessionStatsLabel;
  QProgressBar *m_sourceProgressBar;
  QPushButton *m_cancelRefreshBtn;
  QPushButton *m_refreshSourcesBtn;
  QAction *m_refreshSourcesAction;

  bool m_isRefreshingSources;
  int m_sourcesLoadedCount;
  int m_sourcesAddedCount;
  int m_pagesLoadedCount;
  QTimer *m_sessionRefreshTimer;
  QDateTime m_lastSessionRefreshTime;

  QTimer *m_queueTimer;
  bool m_isProcessingQueue;
  QDateTime m_queueBackoffUntil;
};

#endif // MAINWINDOW_H
