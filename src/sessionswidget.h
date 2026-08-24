#ifndef SESSIONSWIDGET_H
#define SESSIONSWIDGET_H

#include <QJsonObject>
#include <QSortFilterProxyModel>
#include <QWidget>
#include <functional>

class APIManager;
class SessionModel;
class ErrorsModel;
class QTreeView;
class QLabel;
class QProgressBar;
class QPushButton;
class QJsonArray;
class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QActionGroup;
class QAction;

class SessionsProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionsProxyModel(QObject *parent = nullptr);

  void setSourceFilter(const QString &source);
  QString sourceFilter() const;
  void setTextFilter(const QString &text);
  void setStatusFilter(const QString &status);
  void setRepoFilter(const QString &repo);

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
  bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
  QString m_sourceFilter;
  QString m_textFilter;
  QString m_statusFilter;
  QString m_repoFilter;
};

class SessionsWidget : public QWidget {
  Q_OBJECT

public:
  explicit SessionsWidget(const QString &filterSource = QString(), APIManager *apiManager = nullptr,
                          SessionModel *managedModel = nullptr, ErrorsModel *errorsModel = nullptr, QWidget *parent = nullptr);
  ~SessionsWidget() override;

  SessionsProxyModel *proxyModel() const;
  SessionModel *model() const;
  QTreeView *listView() const;

  void setAutoLoadBehavior(QActionGroup *autoLoadGroup);
  void setAutoFollowOnRefresh(bool autoFollow);

Q_SIGNALS:
  void watchRequested(const QJsonObject &sessionData);
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);
  void canResumeChanged(bool canResume);
  void statusMessage(const QString &message);
  void actionStatesChanged(bool canWatch, bool canArchive, bool canDelete);

public Q_SLOTS:
  void refreshSessions();
  void resumeRefresh();
  void loadRemainingRefresh();
  void cancelRefresh();
  void toggleFavourite();
  void increaseFavouriteRank();
  void decreaseFavouriteRank();
  void setFavouriteRank();
  void watchSelectedSessions();
  void archiveSelectedSessions();
  void unmanageSelectedSessions();
  void openSessionUrls();
  void copySessionUrls();
  void openSourceUrls();
  void copySourceUrls();
  void openPrUrls();
  void copyPrUrls();
  void reloadSelectedSessions();
  void copyJulesIds();
  void focusFilter();

private Q_SLOTS:
  void onSessionsReceived(const QJsonArray &sessions, const QString &nextPageToken);
  void onSessionsRefreshFinished();
  void updateRepoFilterList();
  void showContextMenu(const QPoint &pos);
  void onVerticalScrollBarValueChanged(int value);
  void onListViewDoubleClicked(const QModelIndex &index);
  void updateActionStates();

private:
  void applyFavouriteAction(std::function<void(const QString &)> action);

  void setupUi();
  void setupFilters(QVBoxLayout *layout);
  void setupListView();
  QString getSourceUrl(const QModelIndex &idx) const;

  APIManager *m_apiManager;
  ErrorsModel *m_errorsModel;
  SessionModel *m_model;
  SessionModel *m_managedModel;
  SessionsProxyModel *m_proxyModel;
  QTreeView *m_listView;
  QLineEdit *m_searchEdit;
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
  QActionGroup *m_autoLoadGroup;
  bool m_autoFollow = false;
};

#endif // SESSIONSWIDGET_H
