#ifndef SOURCEWINDOW_H
#define SOURCEWINDOW_H

#include "api/apierror.h"
#include <KXmlGuiWindow>

class SourceModel;
class SessionModel;
class QueueModel;
class ErrorsModel;
class QComboBox;
class QLabel;
class ClickableLabel;
class BlockedTreeModel;
class APIManager;
class SessionsWidget;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QListWidget;
class QTextEdit;
class QTreeView;
class QStandardItemModel;

class SourceWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SourceWindow(const QString &sourceId, SourceModel *sourceModel, SessionModel *sessionModel,
                        SessionModel *archiveModel, QueueModel *queueModel, ErrorsModel *errorsModel,
                        BlockedTreeModel *blockedTreeModel, APIManager *apiManager, QWidget *parent = nullptr);
  ~SourceWindow() override;

Q_SIGNALS:
  void newSessionRequested(const QString &sourceId);
  void newSessionFromIssueRequested(const QString &sourceId, const QJsonObject &initialData);
  void queueProcessingRequested();
  void statusMessage(const QString &message);

private:
  void setupUi();
  void setupFollowingTab();
  void setupArchivedTab();
  void setupQueuedBlockedTab();
  void setupSessionsTab();
  void setupSettingsTab();
  void setupRawDataTab();
  void populateDefaultBranches();
  void setupGithubIssuesTab();
  void setupGithubPRsTab();

  static QString generateGithubIssuePrompt(const QString &sourceName, const QString &owner, const QString &repository,
                                           const QJsonObject &issue, const QJsonArray &comments);

private Q_SLOTS:
  void onGithubIssuesReceived(const QString &sourceId, const QJsonArray &issues);
  void updateGithubIssuesDisplay(const QJsonArray &issues);
  void markVisibleSourceErrorsSeen();
  void onGithubPullRequestsReceived(const QString &sourceId, const QJsonArray &prs);
  void onGithubIssueContextReceived(const QString &sourceId, int issueNumber, const QJsonObject &issue,
                                    const QJsonArray &comments);
  void onGithubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error);

private:
  QString m_sourceId;
  SourceModel *m_sourceModel;
  SessionModel *m_sessionModel;
  SessionModel *m_archiveModel;
  QueueModel *m_queueModel;
  ErrorsModel *m_errorsModel;
  BlockedTreeModel *m_blockedTreeModel;
  APIManager *m_apiManager;

  QTabWidget *m_tabWidget;
  QWidget *m_queuedBlockedTab = nullptr;
  QTabWidget *m_subTabWidget = nullptr;
  QWidget *m_errorTab = nullptr;
  SessionsWidget *m_sessionsWidget;
  QCheckBox *m_autoFollowCheckBox;
  QSpinBox *m_concurrencySpinBox;
  QListWidget *m_defaultBranchesList;
  QTextEdit *m_rawDataEdit;

  QTreeView *m_issuesView;
  QTreeView *m_prsView;
  QComboBox *m_issuesStateCombo = nullptr;
  QStandardItemModel *m_issuesModel;
  QStandardItemModel *m_prsModel;
  QAction *m_createSessionFromIssueAction = nullptr;
  QAction *m_cancelFetchAction = nullptr;
  int m_fetchingIssueNumber = -1;
  ClickableLabel *m_statusLabel = nullptr;
  ClickableLabel *m_unseenErrorLabel = nullptr;
};

#endif // SOURCEWINDOW_H
