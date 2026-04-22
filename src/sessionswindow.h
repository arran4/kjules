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
class QComboBox;

class SessionsProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionsProxyModel(QObject *parent = nullptr);

  void setTextFilter(const QString &text);
  void setStatusFilter(const QString &status);
  void setRepoFilter(const QString &repo);

protected:
  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override;
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override;

private:
  QString m_textFilter;
  QString m_statusFilter;
  QString m_repoFilter;
};

class SessionsWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionsWindow(const QString &filterSource = QString(),
                          APIManager *apiManager = nullptr,
                          SessionModel *managedModel = nullptr,
                          QWidget *parent = nullptr);
  ~SessionsWindow();

Q_SIGNALS:
  void watchRequested(const QJsonObject &sessionData);
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);

private Q_SLOTS:
  void refreshSessions();
  void resumeRefresh();
  void loadRemainingRefresh();
  void cancelRefresh();
  void onSessionsReceived(const QJsonArray &sessions,
                          const QString &nextPageToken);
  void onSessionsRefreshFinished();
  void updateRepoFilterList();
  void toggleFavourite();
  void increaseFavouriteRank();
  void decreaseFavouriteRank();
  void setFavouriteRank();
  void updateActionStates();

private:
  template <typename ActionFunc> void applyFavouriteAction(ActionFunc action);

  void setupUi();

  APIManager *m_apiManager;
  SessionModel *m_model;
  SessionModel *m_managedModel;
  SessionsProxyModel *m_proxyModel;
  QTreeView *m_listView;
  QLabel *m_statusLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_cancelBtn;
  QComboBox *m_repoCombo;
  QString m_filterSource;
  int m_sessionsLoaded;
  bool m_isRefreshing;
  int m_pagesLoaded;
  bool m_isRefreshingAll;
  QString m_nextPageToken;
  QAction *m_resumeAction;
  QAction *m_loadRemainingAction;
  QActionGroup *m_autoLoadGroup;
  QAction *m_autoFollowAction = nullptr;
};

#endif // SESSIONSWINDOW_H
