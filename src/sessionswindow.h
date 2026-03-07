#ifndef SESSIONSWINDOW_H
#define SESSIONSWINDOW_H

#include <KXmlGuiWindow>

class APIManager;
class SessionModel;
class QListView;
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
  void cancelRefresh();
  void onSessionsReceived(const QJsonArray &sessions);
  void onSessionsRefreshFinished();

private:
  void setupUi();

  APIManager *m_apiManager;
  SessionModel *m_model;
  QSortFilterProxyModel *m_proxyModel;
  QListView *m_listView;
  QLabel *m_statusLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_cancelBtn;
  QString m_filterSource;
  int m_sessionsLoaded;
  bool m_isRefreshing;
};

#endif // SESSIONSWINDOW_H
