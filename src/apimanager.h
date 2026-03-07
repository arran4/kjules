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

  void setApiKey(const QString &key);
  QString apiKey() const;
  void setGithubToken(const QString &token);
  QString githubToken() const;

  void loadApiKeyFromWallet();
  void saveApiKeyToWallet(const QString &key);
  void loadGithubTokenFromWallet();
  void saveGithubTokenToWallet(const QString &token);

  void testConnection(const QString &apiKey = QString());
  void listSources(const QString &pageToken = QString());
  void cancelListSources();
  void createSession(const QString &source, const QString &prompt,
                     const QString &automationMode = QString());
  void createSessionAsync(const QJsonObject &requestData);
  void listSessions();
  void getSession(const QString &sessionId);
  void getSource(const QString &sourceId);

Q_SIGNALS:
  void sourcesReceived(const QJsonArray &sources);
  void sourcesRefreshFinished();
  void sessionCreated(const QJsonObject &session);
  void sessionsReceived(const QJsonArray &sessions);
  void sessionDetailsReceived(const QJsonObject &session);
  void sourceDetailsReceived(const QJsonObject &source);
  void connectionTested(bool success, const QString &message);
  void errorOccurred(const QString &message);
  void errorOccurredWithResponse(const QString &message,
                                 const QString &response);
  void sessionCreationFailed(const QJsonObject &request,
                             const QJsonObject &response,
                             const QString &errorString);
  void logMessage(const QString &message);

private Q_SLOTS:
  void onWalletOpened(bool success);

private:
  QNetworkAccessManager *m_nam;
  QString m_apiKey;
  QString m_githubToken;
  KWallet::Wallet *m_wallet;
  bool m_tokenFailed;
  QNetworkReply *m_listSourcesReply;

  QNetworkRequest createRequest(const QString &endpoint,
                                const QString &overrideApiKey = QString());
  bool canConnect() const;
};

#endif // APIMANAGER_H
