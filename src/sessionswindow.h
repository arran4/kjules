#ifndef SESSIONSWINDOW_H
#define SESSIONSWINDOW_H

#include <KXmlGuiWindow>
#include <QSortFilterProxyModel>

class APIManager;
class QActionGroup;
class SessionModel;
class QTreeView;
class QLabel;
class QProgressBar;
class QPushButton;
class QJsonArray;

class SessionsProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionsProxyModel(QObject *parent = nullptr);

  void setTextFilter(const QString &text);
  void setStatusFilter(const QString &status);

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  QString m_textFilter;
  QString m_statusFilter;
};

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
  SessionsProxyModel *m_proxyModel;
  QTreeView *m_listView;
  QLabel *m_statusLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_cancelBtn;
  QString m_filterSource;
  int m_sessionsLoaded;
  bool m_isRefreshing;
  QString m_nextPageToken;
  QAction *m_resumeAction;
  QActionGroup *m_autoLoadGroup;
};

#endif // SESSIONSWINDOW_H
