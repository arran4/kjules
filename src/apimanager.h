#ifndef APIMANAGER_H
#define APIMANAGER_H

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
  void testConnection(const QString &apiKey = QString());
  void testGithubConnection(const QString &token = QString());
  QString githubUsername() const;
  QString githubScopes() const;
  void listSources(const QString &pageToken = QString());
  void cancelListSources();
  void createSessionAsync(const QJsonObject &requestData);
  void listSessions(const QString &pageToken = QString());
  void cancelListSessions();
  void getSession(const QString &sessionId);
  void reloadSession(const QString &sessionId);
  static QString cleanSessionId(const QString &sessionId);
  void getSource(const QString &sourceId);
  void listActivities(const QString &sessionId);
  void sendMessage(const QString &sessionId, const QString &message);
  void fetchGithubInfo(const QString &sourceId);
  void fetchGithubBranches(const QString &sourceId);
  void fetchGithubPullRequest(const QString &prUrl);
  void createGithubRepoAsync(const QJsonObject &requestData);

Q_SIGNALS:
  void githubUsernameFetched(const QString &username);
  void githubRepoCreated(const QJsonObject &requestData, const QJsonObject &response);
  void githubRepoCreationFailed(const QJsonObject &requestData, const QJsonObject &response,
                                const QString &errorString);

  void githubInfoReceived(const QString &sourceId, const QJsonObject &info);
  void githubInfoFailed(const QString &sourceId, const QString &message);
  void githubBranchesReceived(const QString &sourceId, const QJsonArray &branches);
  void githubPullRequestInfoReceived(const QString &prUrl, const QJsonObject &info);
  void githubPullRequestFailed(const QString &prUrl, const QString &message);
  void sourcesReceived(const QJsonArray &sources);
  void sourcesRefreshFinished();
  void sessionsRefreshFinished();
  void sessionCreated(const QJsonObject &session);
  void sessionsReceived(const QJsonArray &sessions, const QString &nextPageToken);
  void sessionDetailsReceived(const QJsonObject &session);
  void sessionReloaded(const QJsonObject &session);
  void sessionReloadFailed(const QString &sessionId, const QString &message);
  void sourceDetailsReceived(const QJsonObject &source);
  void activitiesReceived(const QString &sessionId, const QJsonArray &activities);
  void connectionTested(bool success, const QString &message);
  void githubConnectionTested(bool success, const QString &message);
  void errorOccurred(const QString &message);
  void errorOccurredWithResponse(const QString &message, const QString &response);
  void sessionCreationFailed(const QJsonObject &request, const QJsonObject &response, const QString &errorString,
                             const QString &httpDetails);
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

  bool checkGithubRateLimit();
  void updateGithubRateLimit(QNetworkReply *reply);

  QNetworkRequest createRequest(const QString &endpoint, const QString &overrideApiKey = QString());
};

#endif // APIMANAGER_H
