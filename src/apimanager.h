#ifndef APIMANAGER_H
#define APIMANAGER_H

#include "api/apierror.h"
#include <KWallet>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QStandardPaths>
#include <QUrlQuery>

class APIManager : public QObject {
  Q_OBJECT

public:
  explicit APIManager(QObject *parent = nullptr);
  ~APIManager();

  static QString julesSessionBaseUrl() { return QStringLiteral("https://jules.google.com/session/"); }

  void setApiKey(const QString &key);
  QString apiKey() const;
  void setGithubToken(const QString &token);
  QString githubToken() const;

  void setBaseUrl(const QString &url);

  void loadApiKeyFromWallet();
  void saveApiKeyToWallet(const QString &key);
  void saveGithubTokenToWallet(const QString &token);

  bool canConnect() const;
  bool canConnectGithub() const;
  void testConnection(const QString &apiKey = QString());
  void testGithubConnection(const QString &token = QString());
  QString githubUsername() const;
  QString githubScopes() const;
  void listSources(const QString &pageToken = QString(), bool isBackground = false);
  void cancelListSources();
  void createSessionAsync(const QJsonObject &requestData);
  void listSessions(const QString &pageToken = QString(), bool isBackground = false);
  void cancelListSessions();
  void getSession(const QString &sessionId, bool isBackground = false);
  void reloadSession(const QString &sessionId, bool isBackground = false);
  static QString cleanSessionId(const QString &sessionId);
  void getSource(const QString &sourceId, bool isBackground = false);
  void listActivities(const QString &sessionId);
  void sendMessage(const QString &sessionId, const QString &message);
  void fetchGithubInfo(const QString &sourceName, const QString &owner, const QString &repository);
  void fetchGithubBranches(const QString &sourceName, const QString &owner, const QString &repository);
  void fetchGithubIssueContext(const QString &sourceId, const QString &owner, const QString &repository,
                               int issueNumber);
  void cancelGithubIssueContextFetch(const QString &sourceId, int issueNumber);
  void fetchGithubPullRequest(const QString &prUrl);
  void fetchGithubIssues(const QString &sourceId, const QString &owner, const QString &repository,
                         const QString &state = QStringLiteral("open"));
  void fetchGithubPullRequests(const QString &sourceId, const QString &owner, const QString &repository);
  void createGithubRepoAsync(const QJsonObject &requestData);

Q_SIGNALS:
  void githubAvailabilityChanged(bool available);
  void githubUsernameFetched(const QString &username);
  void githubRepoCreated(const QJsonObject &requestData, const QJsonObject &response);
  void githubRepoCreationFailed(const QJsonObject &requestData, const ApiError &apiError);

  void githubInfoReceived(const QString &sourceId, const QJsonObject &info);
  void githubInfoFailed(const QString &sourceId, const QString &message);
  void githubBranchesReceived(const QString &sourceId, const QJsonArray &branches);
  void githubPullRequestInfoReceived(const QString &prUrl, const QJsonObject &info);
  void githubPullRequestFailed(const QString &prUrl, const QString &message);
  void githubIssuesReceived(const QString &sourceId, const QJsonArray &issues);
  void githubPullRequestsReceived(const QString &sourceId, const QJsonArray &prs);
  void githubIssueContextReceived(const QString &sourceId, int issueNumber, const QJsonObject &issue,
                                  const QJsonArray &comments);
  void githubIssueContextFailed(const QString &sourceId, int issueNumber, const ApiError &error);
  void sourcesReceived(const QJsonArray &sources);
  void sourcesRefreshFinished(bool complete);
  void sessionsRefreshFinished();
  void sessionCreated(const QJsonObject &session);
  void sessionsReceived(const QJsonArray &sessions, const QString &nextPageToken);
  void sessionDetailsReceived(const QJsonObject &session);
  void sessionDetailsFailed(const QString &sessionId, const QString &message, bool isBackground);
  void sessionReloaded(const QJsonObject &session);
  void sessionReloadFailed(const QString &sessionId, const QString &message, bool isBackground);
  void sourceDetailsReceived(const QJsonObject &source);
  void activitiesReceived(const QString &sessionId, const QJsonArray &activities);
  void connectionTested(bool success, const QString &message);
  void githubConnectionTested(bool success, const QString &message);
  void errorOccurred(const QString &message, bool isBackground);
  void errorOccurredWithResponse(const QString &message, const QString &response, bool isBackground);
  void sessionCreationFailed(const QJsonObject &request, const ApiError &apiError, const QString &httpDetails);
  void messageSent(const QString &sessionId);
  void messageSendFailed(const QString &sessionId, const QString &message, const QString &httpDetails);
  void logMessage(const QString &message);

private Q_SLOTS:
  void onWalletOpened(bool success);

private:
  QNetworkAccessManager *m_nam;
  QString m_apiKey;
  QString m_githubToken;
  QString m_testedGithubToken;
  QString m_githubUsername;
  QString m_githubScopes;
  QString m_baseUrl;
  KWallet::Wallet *m_wallet;
  bool m_tokenFailed;
  bool m_githubTokenFailed;
  qint64 m_githubRateLimitReset;
  int m_githubRateLimitRemaining;
  QNetworkReply *m_listSourcesReply;
  QNetworkReply *m_listSessionsReply;
  QMap<QString, QNetworkReply *> m_githubIssueContextReplies;

  bool checkGithubRateLimit();
  void updateGithubRateLimit(QNetworkReply *reply);

  QNetworkRequest createRequest(const QString &endpoint, const QString &overrideApiKey = QString());

  void fetchGithubPaginated(const QUrl &initialUrl, const QString &sourceId, bool isIssues);
  void continueGithubPaginated(const QUrl &url, const QString &sourceId, bool isIssues,
                               QSharedPointer<QJsonArray> results);
};

#endif // APIMANAGER_H
