#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QSystemTrayIcon>

class APIManager;
class SessionModel;
class SourceModel;
class DraftsModel;
class ErrorsModel;
class QListView;
class QTreeView;
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
  void showNewSessionDialog();
  void showSettingsDialog();
  void onSessionCreated(const QStringList &sources, const QString &prompt,
                        const QString &automationMode);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
  void onErrorActivated(const QModelIndex &index);
  void onSessionCreationFailed(const QJsonObject &request,
                               const QJsonObject &response,
                               const QString &errorString);
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

private:
  void setupUi();
  void setupTrayIcon();
  void createActions();

  APIManager *m_apiManager;
  SessionModel *m_sessionModel;
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;
  ErrorsModel *m_errorsModel;

  QTreeView *m_sourceView;
  QListView *m_sessionView;
  QListView *m_draftsView;
  QListView *m_errorsView;
  KStatusNotifierItem *m_trayIcon;
  QLabel *m_statusLabel;
  QProgressBar *m_sourceProgressBar;
  QPushButton *m_cancelRefreshBtn;
  QAction *m_refreshSourcesAction;
  QAction *m_showFullSessionListAction;
  QAction *m_viewSessionsAction;
  QAction *m_showPastNewSessionsAction;
  QAction *m_viewRawDataAction;
  QAction *m_openUrlAction;
  QAction *m_copyUrlAction;

  bool m_isRefreshingSources;
  int m_sourcesLoadedCount;
  int m_sourcesAddedCount;
  int m_pagesLoadedCount;
};

#endif // MAINWINDOW_H
