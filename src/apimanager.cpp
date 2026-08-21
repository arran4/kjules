#include "apimanager.h"
#include "sessionrequestbuilder.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <KWallet>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkReply>
#include <QSaveFile>
#include <QtConcurrent>

namespace {
QMutex s_sessionCacheMutex;
} // namespace

const QString DEFAULT_BASE_URL = QStringLiteral("https://jules.googleapis.com/v1alpha");

APIManager::APIManager(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_baseUrl(DEFAULT_BASE_URL), m_wallet(nullptr),
      m_tokenFailed(false), m_githubTokenFailed(false), m_githubRateLimitReset(0), m_githubRateLimitRemaining(-1),
      m_listSourcesReply(nullptr), m_listSessionsReply(nullptr) {
  loadApiKeyFromWallet();
}

APIManager::~APIManager() {
  if (m_listSourcesReply) {
    m_listSourcesReply->abort();
    m_listSourcesReply->deleteLater();
    m_listSourcesReply = nullptr;
  }
  if (m_listSessionsReply) {
    m_listSessionsReply->abort();
    m_listSessionsReply->deleteLater();
    m_listSessionsReply = nullptr;
  }
  if (m_wallet) {
    delete m_wallet;
  }
}

void APIManager::setApiKey(const QString &key) {
  m_apiKey = key;
  m_tokenFailed = false;
  saveApiKeyToWallet(key);
}

QString APIManager::apiKey() const { return m_apiKey; }

void APIManager::setBaseUrl(const QString &url) { m_baseUrl = url; }

void APIManager::setGithubToken(const QString &token) {
  m_githubToken = token;
  m_githubTokenFailed = false;
  saveGithubTokenToWallet(token);
}

QString APIManager::githubToken() const { return m_githubToken; }

void APIManager::loadApiKeyFromWallet() {
  m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
  if (m_wallet) {
    connect(m_wallet, &KWallet::Wallet::walletOpened, this, &APIManager::onWalletOpened);
  }
}

void APIManager::saveApiKeyToWallet(const QString &key) {
  if (m_wallet && m_wallet->isOpen()) {
    m_wallet->writePassword(QStringLiteral("jules_api_key"), key);
  }
}

void APIManager::saveGithubTokenToWallet(const QString &token) {
  if (m_wallet && m_wallet->isOpen()) {
    m_wallet->writePassword(QStringLiteral("github_token"), token);
  }
}

void APIManager::onWalletOpened(bool success) {
  if (success && m_wallet) {
    if (!m_wallet->hasFolder(QStringLiteral("kjules"))) {
      m_wallet->createFolder(QStringLiteral("kjules"));
    }
    m_wallet->setFolder(QStringLiteral("kjules"));
    QString key;
    if (m_wallet->readPassword(QStringLiteral("jules_api_key"), key) == 0) {
      m_apiKey = key;
      m_tokenFailed = false;
      Q_EMIT logMessage(QStringLiteral("API Key loaded from KWallet"));
    }
    QString token;
    if (m_wallet->readPassword(QStringLiteral("github_token"), token) == 0) {
      m_githubToken = token;
      Q_EMIT logMessage(QStringLiteral("GitHub Token loaded from KWallet"));
    }
  } else {
    Q_EMIT errorOccurred(QStringLiteral("Failed to open KWallet"));
  }
}

QNetworkRequest APIManager::createRequest(const QString &endpoint, const QString &overrideApiKey) {
  QNetworkRequest request(QUrl(m_baseUrl + endpoint));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  QString key = overrideApiKey.isEmpty() ? m_apiKey : overrideApiKey;
  key.remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
  if (!key.isEmpty()) {
    request.setRawHeader("X-Goog-Api-Key", key.toUtf8());
  }
  return request;
}

bool APIManager::canConnect() const { return !m_apiKey.isEmpty() && !m_tokenFailed; }

bool APIManager::canConnectGithub() const { return !m_githubToken.isEmpty() && !m_githubTokenFailed; }

QString APIManager::githubUsername() const { return m_githubUsername; }

QString APIManager::githubScopes() const { return m_githubScopes; }

void APIManager::testGithubConnection(const QString &token) {
  QString tk = token.isEmpty() ? m_githubToken : token;
  if (tk.isEmpty()) {
    Q_EMIT githubConnectionTested(false, QStringLiteral("No GitHub token provided."));
    return;
  }

  if (tk == m_testedGithubToken && !m_githubTokenFailed) {
    if (!m_githubUsername.isEmpty()) {
      Q_EMIT githubUsernameFetched(m_githubUsername);
    }
    QString msg = QStringLiteral("GitHub API connected successfully (Cached).");
    if (!m_githubScopes.isEmpty()) {
      msg += QStringLiteral("\nToken scopes: ") + m_githubScopes;
      if (!m_githubScopes.contains(QStringLiteral("repo"))) {
        msg += QStringLiteral("\nWarning: The 'repo' scope is missing. You may "
                              "not be able to fetch private repositories.");
      }
    } else {
      msg += QStringLiteral("\nWarning: No scopes detected. If this is a fine-grained token, "
                            "ensure repository read permissions are granted.");
    }
    Q_EMIT githubConnectionTested(true, msg);
    return;
  }

  QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/user")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + tk;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, tk]() {
    updateGithubRateLimit(reply);
    if (reply->error() == QNetworkReply::NoError) {
      QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
      if (doc.isObject()) {
        QString login = doc.object().value(QStringLiteral("login")).toString();
        if (!login.isEmpty()) {
          m_githubUsername = login;
          Q_EMIT githubUsernameFetched(login);
        }
      }

      QString scopes = QString::fromUtf8(reply->rawHeader("X-OAuth-Scopes"));
      m_githubScopes = scopes;
      m_testedGithubToken = tk;
      QString msg = QStringLiteral("GitHub API connected successfully.");
      if (!scopes.isEmpty()) {
        msg += QStringLiteral("\nToken scopes: ") + scopes;
        if (!scopes.contains(QStringLiteral("repo"))) {
          msg += QStringLiteral("\nWarning: The 'repo' scope is missing. You may "
                                "not be able to fetch private repositories.");
        }
      } else {
        msg += QStringLiteral("\nWarning: No scopes detected. If this is a fine-grained token, "
                              "ensure repository read permissions are granted.");
      }
      Q_EMIT githubConnectionTested(true, msg);
      if (tk == m_githubToken) {
        m_githubTokenFailed = false;
      }
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      Q_EMIT githubConnectionTested(false, QStringLiteral("GitHub connection failed: ") + reply->errorString());
      if ((statusCode == 401 || statusCode == 403) && tk == m_githubToken) {
        m_githubTokenFailed = true;
      }
    }
    reply->deleteLater();
  });
}

void APIManager::testConnection(const QString &apiKey) {
  // If apiKey is empty, we are using the stored key.
  if (apiKey.isEmpty() && !canConnect()) {
    Q_EMIT connectionTested(false, QStringLiteral("Connection skipped: No token or previous failure."));
    return;
  }
  QNetworkRequest request = createRequest(QStringLiteral("/sources"), apiKey);
  QNetworkReply *reply = m_nam->get(request);
  // Capture apiKey to know if we used the stored one
  connect(reply, &QNetworkReply::finished, this, [this, reply, apiKey]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.object().contains(QStringLiteral("sources"))) {
        Q_EMIT connectionTested(true, QStringLiteral("Connection successful."));
      } else {
        Q_EMIT connectionTested(false, QStringLiteral("Connection successful but no sources found "
                                                      "or invalid response."));
      }
    } else {
      if (apiKey.isEmpty()) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401 || statusCode == 403) {
          m_tokenFailed = true;
        }
      }
      Q_EMIT connectionTested(false, QStringLiteral("Connection failed: ") + reply->errorString());
    }
    reply->deleteLater();
  });
}

void APIManager::sendMessage(const QString &sessionId, const QString &message) {
  if (!canConnect()) {
    Q_EMIT messageSendFailed(sessionId, QStringLiteral("No token or previous failure."), QString());
    return;
  }

  QString cleanId = cleanSessionId(sessionId);

  if (cleanId.contains(QStringLiteral("..")) || cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT messageSendFailed(sessionId, QStringLiteral("Invalid session ID."), QString());
    return;
  }

  QString endpoint = QStringLiteral("/sessions/") + cleanId + QStringLiteral(":sendMessage");

  QNetworkRequest request = createRequest(endpoint);

  const QJsonObject json = SessionRequestBuilder::sendMessage(message);
  QByteArray data = QJsonDocument(json).toJson();

  QNetworkReply *reply = m_nam->post(request, data);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionId, request, data]() {
    if (reply->error() == QNetworkReply::NoError) {
      Q_EMIT messageSent(sessionId);
      Q_EMIT logMessage(QStringLiteral("Message sent successfully."));
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }

      QString method = QStringLiteral("POST");
      QString url = reply->url().toString();
      QString httpReq = method + QStringLiteral(" ") + url + QStringLiteral("\n");
      const auto reqHeaders = request.rawHeaderList();
      for (const QByteArray &h : reqHeaders) {
        if (h.toLower() != QByteArrayLiteral("x-goog-api-key") && h.toLower() != QByteArrayLiteral("authorization")) {
          httpReq += QString::fromUtf8(h) + QStringLiteral(": ") + QString::fromUtf8(request.rawHeader(h)) +
                     QStringLiteral("\n");
        } else {
          httpReq += QString::fromUtf8(h) + QStringLiteral(": [REDACTED]\n");
        }
      }
      httpReq += QStringLiteral("\n") + QString::fromUtf8(data);

      QString httpRes = QStringLiteral("HTTP %1 %2\n").arg(statusCode).arg(reply->errorString());
      const auto resHeaders = reply->rawHeaderList();
      for (const QByteArray &h : resHeaders) {
        httpRes +=
            QString::fromUtf8(h) + QStringLiteral(": ") + QString::fromUtf8(reply->rawHeader(h)) + QStringLiteral("\n");
      }

      QByteArray responseData = reply->readAll();
      httpRes += QStringLiteral("\n") + QString::fromUtf8(responseData);

      QString httpDetails =
          QStringLiteral("=== Request ===\n") + httpReq + QStringLiteral("\n\n=== Response ===\n") + httpRes;

      QString reason = reply->errorString();
      QString errorMsg = QStringLiteral("Failed to send message: ") + reason;
      Q_EMIT messageSendFailed(sessionId, reason, httpDetails);
      Q_EMIT errorOccurred(errorMsg);
    }
    reply->deleteLater();
  });
}

void APIManager::listActivities(const QString &sessionId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral("Cannot fetch activities: No token or previous failure."));
    return;
  }

  QString cleanId = cleanSessionId(sessionId);

  if (cleanId.contains(QStringLiteral("..")) || cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid session ID."));
    return;
  }

  QString endpoint = QStringLiteral("/sessions/") + cleanId + QStringLiteral("/activities");

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionId]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      QJsonArray activities;
      if (doc.isObject() && doc.object().contains(QStringLiteral("activities"))) {
        activities = doc.object().value(QStringLiteral("activities")).toArray();
      } else if (doc.isArray()) {
        activities = doc.array();
      }
      Q_EMIT activitiesReceived(sessionId, activities);
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to fetch activities: ") + reply->errorString());
    }
    reply->deleteLater();
  });
}

QString APIManager::cleanSessionId(const QString &sessionId) {
  QString cleanId = sessionId;
  static const QString s_sessions = QStringLiteral("sessions/");
  static const QString s_slashSessions = QStringLiteral("/sessions/");
  static const QString s_session = QStringLiteral("session/");
  static const QString s_slashSession = QStringLiteral("/session/");

  if (cleanId.startsWith(s_sessions)) {
    cleanId = cleanId.mid(s_sessions.length());
  } else if (cleanId.startsWith(s_slashSessions)) {
    cleanId = cleanId.mid(s_slashSessions.length());
  } else if (cleanId.startsWith(s_session)) {
    cleanId = cleanId.mid(s_session.length());
  } else if (cleanId.startsWith(s_slashSession)) {
    cleanId = cleanId.mid(s_slashSession.length());
  } else if (cleanId.startsWith(QStringLiteral("/"))) {
    cleanId = cleanId.mid(1);
  }
  return cleanId;
}

void APIManager::reloadSession(const QString &sessionId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral("Cannot reload session details: No token or previous failure."));
    Q_EMIT sessionReloadFailed(sessionId,
                               QStringLiteral("Cannot reload session details: No token or previous failure."));
    return;
  }

  QString cleanId = cleanSessionId(sessionId);

  if (cleanId.contains(QStringLiteral("..")) || cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid session ID."));
    Q_EMIT sessionReloadFailed(sessionId, QStringLiteral("Invalid session ID."));
    return;
  }

  QString endpoint = QStringLiteral("/sessions/") + cleanId;

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionId]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      Q_EMIT sessionReloaded(doc.object());
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      QString errorMsg = QStringLiteral("Failed to reload session details: ") + reply->errorString();
      Q_EMIT errorOccurred(errorMsg);
      Q_EMIT sessionReloadFailed(sessionId, errorMsg);
    }
    reply->deleteLater();
  });
}

void APIManager::getSource(const QString &sourceId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral("Cannot get source details: No token or previous failure."));
    return;
  }

  if (!sourceId.startsWith(QStringLiteral("sources/")) || sourceId.contains(QStringLiteral(".."))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid source ID."));
    return;
  }

  const QString endpoint = QLatin1Char('/') + sourceId;

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      Q_EMIT sourceDetailsReceived(doc.object());
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to get source details: ") + reply->errorString());
    }
    reply->deleteLater();
  });
}

void APIManager::listSources(const QString &pageToken) {
  if (!canConnect()) {
    Q_EMIT logMessage(QStringLiteral("Skipping listSources: No token or previous failure."));
    Q_EMIT sourcesRefreshFinished(false);
    return;
  }

  if (m_listSourcesReply) {
    // If a request is already in progress, ignore new request.
    return;
  }

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("API"));
  int pageSize = config.readEntry("PageSize", 100);
  QString endpoint = QStringLiteral("/sources?pageSize=") + QString::number(pageSize);
  if (!pageToken.isEmpty()) {
    endpoint += QStringLiteral("&pageToken=") + pageToken;
  }
  QNetworkRequest request = createRequest(endpoint);
  m_listSourcesReply = m_nam->get(request);

  connect(m_listSourcesReply, &QNetworkReply::finished, this, [this]() {
    QNetworkReply *reply = m_listSourcesReply;
    m_listSourcesReply = nullptr;

    if (!reply)
      return; // In case it was already deleted or null

    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      QJsonObject obj = doc.object();
      QJsonArray sources = obj.value(QStringLiteral("sources")).toArray();
      Q_EMIT sourcesReceived(sources);

      QString nextPageToken = obj.value(QStringLiteral("nextPageToken")).toString();
      if (!nextPageToken.isEmpty()) {
        // Fetch next page automatically
        listSources(nextPageToken);
      } else {
        Q_EMIT sourcesRefreshFinished(true);
        Q_EMIT logMessage(QStringLiteral("Sources refreshed successfully."));
      }
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
      Q_EMIT sourcesRefreshFinished(false);
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to list sources: ") + reply->errorString());
      Q_EMIT sourcesRefreshFinished(false);
    }
    reply->deleteLater();
  });
}

void APIManager::cancelListSources() {
  if (m_listSourcesReply) {
    m_listSourcesReply->abort();
    // The finished signal will be emitted with OperationCanceledError,
    // which will emit sourcesRefreshFinished().
  }
}

void APIManager::fetchGithubPullRequest(const QString &prUrl) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed) {
    Q_EMIT githubPullRequestFailed(prUrl, QStringLiteral("GitHub token authentication failed previously."));
    return;
  }
  if (!checkGithubRateLimit()) {
    Q_EMIT githubPullRequestFailed(prUrl, QStringLiteral("Rate limit exhausted"));
    return;
  }

  // prUrl format: https://github.com/owner/repo/pull/123
  if (!prUrl.startsWith(QStringLiteral("https://github.com/"))) {
    return;
  }

  QString path = prUrl.mid(19); // owner/repo/pull/123
  QStringList parts = path.split(QLatin1Char('/'));
  if (parts.size() < 4 || parts[2] != QStringLiteral("pull")) {
    return;
  }

  QString apiUrl = QStringLiteral("https://api.github.com/repos/") + parts[0] + QLatin1Char('/') + parts[1] +
                   QStringLiteral("/pulls/") + parts[3];

  QNetworkRequest request((QUrl(apiUrl)));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, prUrl]() {
    reply->deleteLater();
    updateGithubRateLimit(reply);
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        Q_EMIT githubPullRequestInfoReceived(prUrl, doc.object());
      } else {
        Q_EMIT githubPullRequestFailed(prUrl, QStringLiteral("Invalid JSON response"));
      }
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
        Q_EMIT logMessage(QStringLiteral("GitHub API authentication failed (HTTP %1). Future "
                                         "requests blocked until token is updated.")
                              .arg(statusCode));
      }
      Q_EMIT githubPullRequestFailed(prUrl, reply->errorString());
    }
  });
}

bool APIManager::checkGithubRateLimit() {
  if (m_githubRateLimitRemaining == 0) {
    qint64 currentEpoch = QDateTime::currentSecsSinceEpoch();
    if (currentEpoch < m_githubRateLimitReset) {
      Q_EMIT logMessage(
          QStringLiteral("GitHub API rate limit exhausted. Waiting until %1...").arg(m_githubRateLimitReset));
      return false;
    } else {
      // Reset passed, allow requests to test and fetch new headers
      m_githubRateLimitRemaining = -1;
    }
  }
  return true;
}

void APIManager::createGithubRepoAsync(const QJsonObject &requestData) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed) {
    Q_EMIT githubRepoCreationFailed(
        requestData, ApiError(ApiError::Type::Authentication,
                              QStringLiteral("GitHub token authentication failed previously or not provided.")));
    return;
  }
  if (!checkGithubRateLimit()) {
    Q_EMIT githubRepoCreationFailed(requestData,
                                    ApiError(ApiError::Type::RateLimit, QStringLiteral("Rate limit exhausted")));
    return;
  }

  QString org = requestData.value(QStringLiteral("org")).toString();
  QString endpoint = org.isEmpty() ? QStringLiteral("https://api.github.com/user/repos")
                                   : QStringLiteral("https://api.github.com/orgs/%1/repos").arg(org);

  QNetworkRequest request;
  request.setUrl(QUrl(endpoint));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

  QJsonObject payload;
  payload[QStringLiteral("name")] = requestData.value(QStringLiteral("repoName")).toString();
  payload[QStringLiteral("private")] = requestData.value(QStringLiteral("private")).toBool(true);
  payload[QStringLiteral("auto_init")] = true;

  QJsonDocument payloadDoc(payload);
  QNetworkReply *reply = m_nam->post(request, payloadDoc.toJson());

  connect(reply, &QNetworkReply::finished, this, [this, reply, requestData]() {
    updateGithubRateLimit(reply);

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject response = doc.object();

    if (reply->error() == QNetworkReply::NoError) {
      Q_EMIT githubRepoCreated(requestData, response);
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
        Q_EMIT errorOccurred(QStringLiteral("GitHub API authentication failed (HTTP %1). Future "
                                            "requests will be blocked until the token is verified.")
                                 .arg(statusCode));
      }
      QByteArray responseData = reply->readAll();
      ApiError apiError = ApiErrorDetector::detect(reply, responseData);
      Q_EMIT githubRepoCreationFailed(requestData, apiError);
    }
    reply->deleteLater();
  });
}

void APIManager::updateGithubRateLimit(QNetworkReply *reply) {
  if (reply->hasRawHeader("x-ratelimit-remaining")) {
    m_githubRateLimitRemaining = reply->rawHeader("x-ratelimit-remaining").toInt();
  }
  if (reply->hasRawHeader("x-ratelimit-reset")) {
    m_githubRateLimitReset = reply->rawHeader("x-ratelimit-reset").toLongLong();
  }
}

void APIManager::fetchGithubInfo(const QString &sourceName, const QString &owner, const QString &repository) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed) {
    if (m_githubTokenFailed) {
      Q_EMIT githubInfoFailed(sourceName, QStringLiteral("GitHub token authentication failed previously."));
    }
    return;
  }

  if (!checkGithubRateLimit()) {
    Q_EMIT githubInfoFailed(sourceName, QStringLiteral("Rate limit exhausted"));
    return;
  }

  if (sourceName.isEmpty() || owner.isEmpty() || repository.isEmpty()) {
    Q_EMIT githubInfoFailed(sourceName, QStringLiteral("Missing GitHub repository metadata."));
    return;
  }

  const QString repositoryPath = QString::fromLatin1(QUrl::toPercentEncoding(owner)) + QLatin1Char('/') +
                                 QString::fromLatin1(QUrl::toPercentEncoding(repository));
  QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/") + repositoryPath));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceName]() {
    reply->deleteLater();
    updateGithubRateLimit(reply);
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        Q_EMIT githubInfoReceived(sourceName, doc.object());
      } else {
        Q_EMIT githubInfoFailed(sourceName, QStringLiteral("Invalid JSON response"));
      }
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
        Q_EMIT logMessage(QStringLiteral("GitHub API authentication failed (HTTP %1). Future "
                                         "requests blocked until token is updated.")
                              .arg(statusCode));
      }
      Q_EMIT githubInfoFailed(sourceName, reply->errorString());
    }
  });
}

void APIManager::createSessionAsync(const QJsonObject &requestData) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral("Cannot create session: No token or previous failure."));
    return;
  }

  QNetworkRequest request = createRequest(QStringLiteral("/sessions"));
  const QJsonObject json = SessionRequestBuilder::createSession(requestData);

  QByteArray data = QJsonDocument(json).toJson();
  QNetworkReply *reply = m_nam->post(request, data);

  connect(reply, &QNetworkReply::finished, this, [this, reply, request, json, data, requestData]() {
    QByteArray responseData = reply->readAll();
    if (reply->error() == QNetworkReply::NoError) {
      QJsonDocument doc = QJsonDocument::fromJson(responseData);
      QJsonObject sessionObj = SessionRequestBuilder::sessionResponseWithRequest(doc.object(), json);
      if (requestData.contains(QStringLiteral("ignoreConcurrency"))) {
        sessionObj[QStringLiteral("ignoreConcurrency")] =
            requestData.value(QStringLiteral("ignoreConcurrency")).toBool();
      }

      Q_EMIT sessionCreated(sessionObj);
      Q_EMIT logMessage(QStringLiteral("Session created successfully."));

      // Cache session locally
      QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      auto cacheFuture = QtConcurrent::run([path, sessionObj]() {
        QMutexLocker locker(&s_sessionCacheMutex);
        QDir().mkpath(path);
        QString filePath = path + QStringLiteral("/cached_sessions.json");
        QFile file(filePath);
        QJsonArray cachedSessions;
        if (file.open(QIODevice::ReadOnly)) {
          QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
          cachedSessions = doc.array();
          file.close();
        }
        cachedSessions.append(sessionObj);

        QSaveFile saveFile(filePath);
        if (saveFile.open(QIODevice::WriteOnly)) {
          QJsonDocument writeDoc(cachedSessions);
          saveFile.write(writeDoc.toJson());
          saveFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
          saveFile.commit();
        }
      });
      Q_UNUSED(cacheFuture)
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      QString method = QStringLiteral("POST");
      QString url = reply->url().toString();
      QString httpReq = method + QStringLiteral(" ") + url + QStringLiteral("\n");
      const auto reqHeaders = request.rawHeaderList();
      for (const QByteArray &h : reqHeaders) {
        if (h.toLower() != QByteArrayLiteral("x-goog-api-key") && h.toLower() != QByteArrayLiteral("authorization")) {
          httpReq += QString::fromUtf8(h) + QStringLiteral(": ") + QString::fromUtf8(request.rawHeader(h)) +
                     QStringLiteral("\n");
        } else {
          httpReq += QString::fromUtf8(h) + QStringLiteral(": [REDACTED]\n");
        }
      }
      httpReq += QStringLiteral("\n") + QString::fromUtf8(data);

      QString httpRes = QStringLiteral("HTTP %1 %2\n").arg(statusCode).arg(reply->errorString());
      const auto resHeaders = reply->rawHeaderList();
      for (const QByteArray &h : resHeaders) {
        httpRes +=
            QString::fromUtf8(h) + QStringLiteral(": ") + QString::fromUtf8(reply->rawHeader(h)) + QStringLiteral("\n");
      }

      QString errorStr = reply->errorString();
      // Do not readAll() again, use responseData
      httpRes += QStringLiteral("\n") + QString::fromUtf8(responseData);

      QString httpDetails =
          QStringLiteral("=== Request ===\n") + httpReq + QStringLiteral("\n\n=== Response ===\n") + httpRes;

      ApiError apiError = ApiErrorDetector::detect(reply, responseData);
      Q_EMIT sessionCreationFailed(requestData, apiError, httpDetails);
      QString errorMsg = QStringLiteral("Failed to create session: ") + reply->errorString();
      Q_EMIT errorOccurred(errorMsg);
      Q_EMIT errorOccurredWithResponse(errorMsg, QString::fromUtf8(responseData));
    }
    reply->deleteLater();
  });
}

void APIManager::listSessions(const QString &pageToken) {
  if (!canConnect()) {
    Q_EMIT logMessage(QStringLiteral("Skipping listSessions: No token or previous failure."));
    Q_EMIT sessionsRefreshFinished();
    return;
  }

  if (m_listSessionsReply) {
    return;
  }

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("API"));
  int pageSize = config.readEntry("PageSize", 100);
  QString endpoint = QStringLiteral("/sessions?pageSize=") + QString::number(pageSize);
  if (!pageToken.isEmpty()) {
    endpoint += QStringLiteral("&pageToken=") + pageToken;
  }
  QNetworkRequest request = createRequest(endpoint);
  m_listSessionsReply = m_nam->get(request);

  QNetworkReply *reply = m_listSessionsReply;
  connect(m_listSessionsReply, &QNetworkReply::finished, this, [this, reply]() {
    if (m_listSessionsReply == reply) {
      m_listSessionsReply = nullptr;
    }

    if (!reply)
      return;

    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      QJsonObject obj = doc.object();
      QJsonArray sessions = obj.value(QStringLiteral("sessions")).toArray();
      QString nextPageToken = obj.value(QStringLiteral("nextPageToken")).toString();

      Q_EMIT sessionsReceived(sessions, nextPageToken);

      Q_EMIT sessionsRefreshFinished();
      Q_EMIT logMessage(QStringLiteral("Sessions refreshed successfully."));
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
      Q_EMIT sessionsRefreshFinished();
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to list sessions: ") + reply->errorString());
      Q_EMIT sessionsRefreshFinished();
    }
    reply->deleteLater();
  });
}

void APIManager::cancelListSessions() {
  if (m_listSessionsReply) {
    m_listSessionsReply->abort();
  }
}

void APIManager::getSession(const QString &sessionId) {
  if (!canConnect()) {
    QString msg = QStringLiteral("Cannot get session details: No token or previous failure.");
    Q_EMIT errorOccurred(msg);
    Q_EMIT sessionDetailsFailed(sessionId, msg);
    return;
  }
  // sessionId should be the full resource name e.g. "sessions/123..."
  QString cleanId = sessionId;
  if (cleanId.startsWith(QStringLiteral("sessions/"))) {
    cleanId = cleanId.mid(9);
  } else if (cleanId.startsWith(QStringLiteral("/sessions/"))) {
    cleanId = cleanId.mid(10);
  } else if (cleanId.startsWith(QStringLiteral("/"))) {
    cleanId = cleanId.mid(1);
  }

  if (cleanId.contains(QStringLiteral("..")) || cleanId.contains(QStringLiteral("/"))) {
    QString msg = QStringLiteral("Invalid session ID.");
    Q_EMIT errorOccurred(msg);
    Q_EMIT sessionDetailsFailed(sessionId, msg);
    return;
  }

  QString endpoint = QStringLiteral("/sessions/") + cleanId;

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, cleanId]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      Q_EMIT sessionDetailsReceived(doc.object());
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      QString msg = QStringLiteral("Failed to get session details: ") + reply->errorString();
      Q_EMIT errorOccurred(msg);
      Q_EMIT sessionDetailsFailed(cleanId, msg);
    }
    reply->deleteLater();
  });
}

void APIManager::continueGithubPaginated(const QUrl &url, const QString &sourceId, bool isIssues,
                                         QSharedPointer<QJsonArray> results) {
  if (!checkGithubRateLimit()) {
    return;
  }

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId, isIssues, results]() {
    reply->deleteLater();
    updateGithubRateLimit(reply);

    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isArray()) {
        QJsonArray pageArray = doc.array();
        for (const QJsonValue &v : pageArray) {
          if (isIssues && v.toObject().contains(QStringLiteral("pull_request"))) {
            continue;
          }
          results->append(v);
        }
      }

      QString linkHeader = QString::fromUtf8(reply->rawHeader("Link"));
      QUrl nextUrl;
      if (!linkHeader.isEmpty()) {
        QStringList links = linkHeader.split(QLatin1Char(','));
        for (const QString &link : links) {
          if (link.contains(QStringLiteral("rel=\"next\""))) {
            int start = link.indexOf(QLatin1Char('<'));
            int end = link.indexOf(QLatin1Char('>'));
            if (start != -1 && end != -1 && end > start) {
              nextUrl = QUrl(link.mid(start + 1, end - start - 1));
            }
            break;
          }
        }
      }

      if (nextUrl.isValid()) {
        continueGithubPaginated(nextUrl, sourceId, isIssues, results);
      } else {
        if (isIssues) {
          Q_EMIT githubIssuesReceived(sourceId, *results);
        } else {
          Q_EMIT githubPullRequestsReceived(sourceId, *results);
        }
      }
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
        Q_EMIT logMessage(
            QStringLiteral(
                "GitHub API authentication failed (HTTP %1). Future requests blocked until token is updated.")
                .arg(statusCode));
      }
    }
  });
}

void APIManager::fetchGithubPaginated(const QUrl &initialUrl, const QString &sourceId, bool isIssues) {
  auto results = QSharedPointer<QJsonArray>::create();
  continueGithubPaginated(initialUrl, sourceId, isIssues, results);
}

void APIManager::fetchGithubIssueContext(const QString &sourceId, const QString &owner, const QString &repository,
                                         int issueNumber) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed || !checkGithubRateLimit()) {
    Q_EMIT githubIssueContextFailed(sourceId, issueNumber,
                                    QStringLiteral("No valid GitHub token or rate limit exhausted."));
    return;
  }

  if (sourceId.isEmpty() || owner.isEmpty() || repository.isEmpty()) {
    Q_EMIT githubIssueContextFailed(sourceId, issueNumber, QStringLiteral("Invalid source metadata."));
    return;
  }

  const QString repositoryPath = QString::fromLatin1(QUrl::toPercentEncoding(owner)) + QLatin1Char('/') +
                                 QString::fromLatin1(QUrl::toPercentEncoding(repository));

  QUrl issueUrl(QStringLiteral("https://api.github.com/repos/") + repositoryPath + QStringLiteral("/issues/") +
                QString::number(issueNumber));

  QNetworkRequest request(issueUrl);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId, issueNumber, repositoryPath]() {
    reply->deleteLater();
    updateGithubRateLimit(reply);

    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonObject issueObj = QJsonDocument::fromJson(data).object();

      // Fetch comments paginated
      QUrl commentsUrl(QStringLiteral("https://api.github.com/repos/") + repositoryPath + QStringLiteral("/issues/") +
                       QString::number(issueNumber) + QStringLiteral("/comments?per_page=100"));
      auto comments = QSharedPointer<QJsonArray>::create();

      // We will create a local helper to do the pagination so we can emit when it finishes
      auto fetchComments = [this](auto fetchCommentsRef, QUrl url, const QString &sId, int iNum, QJsonObject iObj,
                                  QSharedPointer<QJsonArray> res) -> void {
        if (!checkGithubRateLimit()) {
          Q_EMIT githubIssueContextFailed(sId, iNum, QStringLiteral("Rate limit exhausted while fetching comments."));
          return;
        }

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
        req.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
        req.setRawHeader("Accept", "application/vnd.github.v3+json");
        QString tkAuth = QStringLiteral("Bearer ") + m_githubToken;
        req.setRawHeader("Authorization", tkAuth.toUtf8());

        QNetworkReply *rep = m_nam->get(req);
        connect(rep, &QNetworkReply::finished, this, [this, rep, fetchCommentsRef, sId, iNum, iObj, res]() {
          rep->deleteLater();
          updateGithubRateLimit(rep);

          if (rep->error() == QNetworkReply::NoError) {
            QJsonArray pageArray = QJsonDocument::fromJson(rep->readAll()).array();
            for (const QJsonValue &v : pageArray) {
              res->append(v);
            }

            QString linkHeader = QString::fromUtf8(rep->rawHeader("Link"));
            QUrl nextUrl;
            if (!linkHeader.isEmpty()) {
              QStringList links = linkHeader.split(QLatin1Char(','));
              for (const QString &link : links) {
                if (link.contains(QStringLiteral("rel=\"next\""))) {
                  int start = link.indexOf(QLatin1Char('<'));
                  int end = link.indexOf(QLatin1Char('>'));
                  if (start != -1 && end != -1 && end > start) {
                    nextUrl = QUrl(link.mid(start + 1, end - start - 1));
                  }
                  break;
                }
              }
            }

            if (nextUrl.isValid()) {
              fetchCommentsRef(fetchCommentsRef, nextUrl, sId, iNum, iObj, res);
            } else {
              Q_EMIT githubIssueContextReceived(sId, iNum, iObj, *res);
            }
          } else {
            int statusCode = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 401 || statusCode == 403) {
              m_githubTokenFailed = true;
            }
            Q_EMIT githubIssueContextFailed(sId, iNum,
                                            QStringLiteral("Failed to fetch comments: ") + rep->errorString());
          }
        });
      };

      fetchComments(fetchComments, commentsUrl, sourceId, issueNumber, issueObj, comments);
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
      }
      Q_EMIT githubIssueContextFailed(sourceId, issueNumber,
                                      QStringLiteral("Failed to fetch issue context: ") + reply->errorString());
    }
  });
}

void APIManager::fetchGithubIssues(const QString &sourceId, const QString &owner, const QString &repository) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed || !checkGithubRateLimit()) {
    return;
  }

  if (sourceId.isEmpty() || owner.isEmpty() || repository.isEmpty())
    return;

  const QString repositoryPath = QString::fromLatin1(QUrl::toPercentEncoding(owner)) + QLatin1Char('/') +
                                 QString::fromLatin1(QUrl::toPercentEncoding(repository));
  QUrl url(QStringLiteral("https://api.github.com/repos/") + repositoryPath +
           QStringLiteral("/issues?state=open&per_page=100"));
  fetchGithubPaginated(url, sourceId, true);
}

void APIManager::fetchGithubPullRequests(const QString &sourceId, const QString &owner, const QString &repository) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed || !checkGithubRateLimit()) {
    return;
  }

  if (sourceId.isEmpty() || owner.isEmpty() || repository.isEmpty())
    return;

  const QString repositoryPath = QString::fromLatin1(QUrl::toPercentEncoding(owner)) + QLatin1Char('/') +
                                 QString::fromLatin1(QUrl::toPercentEncoding(repository));
  QUrl url(QStringLiteral("https://api.github.com/repos/") + repositoryPath +
           QStringLiteral("/pulls?state=all&per_page=100"));
  fetchGithubPaginated(url, sourceId, false);
}

void APIManager::fetchGithubBranches(const QString &sourceName, const QString &owner, const QString &repository) {
  if (m_githubToken.isEmpty() || m_githubTokenFailed || !checkGithubRateLimit()) {
    return;
  }

  if (sourceName.isEmpty() || owner.isEmpty() || repository.isEmpty())
    return;

  const QString repositoryPath = QString::fromLatin1(QUrl::toPercentEncoding(owner)) + QLatin1Char('/') +
                                 QString::fromLatin1(QUrl::toPercentEncoding(repository));
  QNetworkRequest request(
      QUrl(QStringLiteral("https://api.github.com/repos/") + repositoryPath + QStringLiteral("/branches")));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
  request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QStringLiteral("kjules")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceName]() {
    reply->deleteLater();
    updateGithubRateLimit(reply);
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isArray()) {
        Q_EMIT githubBranchesReceived(sourceName, doc.array());
      }
    } else {
      int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_githubTokenFailed = true;
        Q_EMIT logMessage(QStringLiteral("GitHub API authentication failed (HTTP %1). Future "
                                         "requests blocked until token is updated.")
                              .arg(statusCode));
      }
    }
  });
}
