#ifndef SESSIONSWINDOW_H
#define SESSIONSWINDOW_H

#include <KXmlGuiWindow>

class APIManager;
class SessionModel;
class QTreeView;
class QSortFilterProxyModel;
class QLabel;
class QProgressBar;
class QPushButton;
class QJsonArray;

class SessionsWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionsWindow(const QString &filterSource = QString(),
                          APIManager *apiManager = nullptr,
                          QWidget *parent = nullptr);
  ~SessionsWindow();

private Q_SLOTS:
  void refreshSessions();
  void resumeRefresh();
  void cancelRefresh();
  void onSessionsReceived(const QJsonArray &sessions, const QString &nextPageToken);
  void onSessionsRefreshFinished();

private:
  void setupUi();

  APIManager *m_apiManager;
  SessionModel *m_model;
  QSortFilterProxyModel *m_proxyModel;
  QTreeView *m_listView;
  QLabel *m_statusLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_cancelBtn;
  QString m_filterSource;
  int m_sessionsLoaded;
  bool m_isRefreshing;
  QString m_nextPageToken;
  QAction *m_resumeAction;
};

#endif // SESSIONSWINDOW_H
