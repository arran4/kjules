#include "apimanager.h"
#include <KWallet>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

const QString DEFAULT_BASE_URL =
    QStringLiteral("https://jules.googleapis.com/v1alpha");

APIManager::APIManager(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)),
      m_baseUrl(DEFAULT_BASE_URL), m_wallet(nullptr), m_tokenFailed(false),
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

QString APIManager::baseUrl() const { return m_baseUrl; }

void APIManager::setGithubToken(const QString &token) {
  m_githubToken = token;
  saveGithubTokenToWallet(token);
}

QString APIManager::githubToken() const { return m_githubToken; }

void APIManager::loadApiKeyFromWallet() {
  m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0,
                                         KWallet::Wallet::Asynchronous);
  if (m_wallet) {
    connect(m_wallet, &KWallet::Wallet::walletOpened, this,
            &APIManager::onWalletOpened);
  }
}

void APIManager::saveApiKeyToWallet(const QString &key) {
  if (m_wallet && m_wallet->isOpen()) {
    m_wallet->writePassword(QStringLiteral("jules_api_key"), key);
  }
}

void APIManager::loadGithubTokenFromWallet() {
  if (m_wallet && m_wallet->isOpen()) {
    QString token;
    if (m_wallet->readPassword(QStringLiteral("github_token"), token) == 0) {
      m_githubToken = token;
      Q_EMIT logMessage(QStringLiteral("GitHub Token loaded from KWallet"));
    }
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

QNetworkRequest APIManager::createRequest(const QString &endpoint,
                                          const QString &overrideApiKey) {
  QNetworkRequest request(QUrl(m_baseUrl + endpoint));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QVariant(QStringLiteral("application/json")));
  QString key = overrideApiKey.isEmpty() ? m_apiKey : overrideApiKey;
  key.remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
  if (!key.isEmpty()) {
    request.setRawHeader("X-Goog-Api-Key", key.toUtf8());
  }
  return request;
}

bool APIManager::canConnect() const {
  return !m_apiKey.isEmpty() && !m_tokenFailed;
}

void APIManager::testConnection(const QString &apiKey) {
  // If apiKey is empty, we are using the stored key.
  if (apiKey.isEmpty() && !canConnect()) {
    Q_EMIT connectionTested(
        false,
        QStringLiteral("Connection skipped: No token or previous failure."));
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
        Q_EMIT connectionTested(
            false, QStringLiteral("Connection successful but no sources found "
                                  "or invalid response."));
      }
    } else {
      if (apiKey.isEmpty()) {
        int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401 || statusCode == 403) {
          m_tokenFailed = true;
        }
      }
      Q_EMIT connectionTested(false, QStringLiteral("Connection failed: ") +
                                         reply->errorString());
    }
    reply->deleteLater();
  });
}

void APIManager::listActivities(const QString &sessionId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral(
        "Cannot fetch activities: No token or previous failure."));
    return;
  }

  QString cleanId = cleanSessionId(sessionId);

  if (cleanId.contains(QStringLiteral("..")) ||
      cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid session ID."));
    return;
  }

  QString endpoint =
      QStringLiteral("/sessions/") + cleanId + QStringLiteral("/activities");

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionId]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      QJsonArray activities;
      if (doc.isObject() &&
          doc.object().contains(QStringLiteral("activities"))) {
        activities = doc.object().value(QStringLiteral("activities")).toArray();
      } else if (doc.isArray()) {
        activities = doc.array();
      }
      Q_EMIT activitiesReceived(sessionId, activities);
    } else {
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to fetch activities: ") +
                           reply->errorString());
    }
    reply->deleteLater();
  });
}

QString APIManager::cleanSessionId(const QString &sessionId) {
  QString cleanId = sessionId;
  if (cleanId.startsWith(QStringLiteral("sessions/"))) {
    cleanId = cleanId.mid(9);
  } else if (cleanId.startsWith(QStringLiteral("/sessions/"))) {
    cleanId = cleanId.mid(10);
  } else if (cleanId.startsWith(QStringLiteral("session/"))) {
    cleanId = cleanId.mid(8);
  } else if (cleanId.startsWith(QStringLiteral("/session/"))) {
    cleanId = cleanId.mid(9);
  } else if (cleanId.startsWith(QStringLiteral("/"))) {
    cleanId = cleanId.mid(1);
  }
  return cleanId;
}

void APIManager::reloadSession(const QString &sessionId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral(
        "Cannot reload session details: No token or previous failure."));
    return;
  }

  QString cleanId = cleanSessionId(sessionId);

  if (cleanId.contains(QStringLiteral("..")) ||
      cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid session ID."));
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
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      QString errorMsg = QStringLiteral("Failed to reload session details: ") +
                         reply->errorString();
      Q_EMIT errorOccurred(errorMsg);
      Q_EMIT sessionReloadFailed(sessionId, errorMsg);
    }
    reply->deleteLater();
  });
}

void APIManager::getSource(const QString &sourceId) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(QStringLiteral(
        "Cannot get source details: No token or previous failure."));
    return;
  }

  QString cleanId = sourceId;
  if (cleanId.startsWith(QStringLiteral("sources/"))) {
    cleanId = cleanId.mid(8);
  } else if (cleanId.startsWith(QStringLiteral("/sources/"))) {
    cleanId = cleanId.mid(9);
  } else if (cleanId.startsWith(QStringLiteral("/"))) {
    cleanId = cleanId.mid(1);
  }

  if (cleanId.contains(QStringLiteral(".."))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid source ID."));
    return;
  }

  QString endpoint = QStringLiteral("/sources/") + cleanId;

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      Q_EMIT sourceDetailsReceived(doc.object());
    } else {
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to get source details: ") +
                           reply->errorString());
    }
    reply->deleteLater();
  });
}

void APIManager::listSources(const QString &pageToken) {
  if (!canConnect()) {
    Q_EMIT logMessage(
        QStringLiteral("Skipping listSources: No token or previous failure."));
    Q_EMIT sourcesRefreshFinished();
    return;
  }

  if (m_listSourcesReply) {
    // If a request is already in progress, ignore new request.
    return;
  }

  QString endpoint = QStringLiteral("/sources");
  if (!pageToken.isEmpty()) {
    endpoint += QStringLiteral("?pageToken=") + pageToken;
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

      QString nextPageToken =
          obj.value(QStringLiteral("nextPageToken")).toString();
      if (!nextPageToken.isEmpty()) {
        // Fetch next page automatically
        listSources(nextPageToken);
      } else {
        Q_EMIT sourcesRefreshFinished();
        Q_EMIT logMessage(QStringLiteral("Sources refreshed successfully."));
      }
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
      Q_EMIT sourcesRefreshFinished();
    } else {
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to list sources: ") +
                           reply->errorString());
      Q_EMIT sourcesRefreshFinished();
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

void APIManager::createSession(const QString &source, const QString &prompt,
                               const QString &automationMode,
                               bool requirePlanApproval) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(
        QStringLiteral("Cannot create session: No token or previous failure."));
    return;
  }
  QJsonObject requestData;
  requestData[QStringLiteral("source")] = source;
  requestData[QStringLiteral("prompt")] = prompt;
  if (requirePlanApproval) {
    requestData[QStringLiteral("requirePlanApproval")] = true;
  }
  if (!automationMode.isEmpty()) {
    requestData[QStringLiteral("automationMode")] = automationMode;
  }
  createSessionAsync(requestData);
}

void APIManager::fetchGithubPullRequest(const QString &prUrl) {
  if (m_githubToken.isEmpty()) {
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

  QString apiUrl = QStringLiteral("https://api.github.com/repos/") + parts[0] +
                   QLatin1Char('/') + parts[1] + QStringLiteral("/pulls/") +
                   parts[3];

  QNetworkRequest request((QUrl(apiUrl)));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QVariant(QStringLiteral("application/json")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, prUrl]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        Q_EMIT githubPullRequestInfoReceived(prUrl, doc.object());
      } else {
        Q_EMIT githubPullRequestFailed(prUrl,
                                       QStringLiteral("Invalid JSON response"));
      }
    } else {
      Q_EMIT githubPullRequestFailed(prUrl, reply->errorString());
    }
  });
}

void APIManager::fetchGithubInfo(const QString &sourceId) {
  if (m_githubToken.isEmpty()) {
    return;
  }

  QString cleanId = sourceId;
  if (cleanId.startsWith(QStringLiteral("sources/github/"))) {
    cleanId = cleanId.mid(15);
  } else {
    return; // Not a github source
  }

  QNetworkRequest request(
      QUrl(QStringLiteral("https://api.github.com/repos/") + cleanId));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QVariant(QStringLiteral("application/json")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        Q_EMIT githubInfoReceived(sourceId, doc.object());
      }
    }
  });
}

void APIManager::createSessionAsync(const QJsonObject &requestData) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(
        QStringLiteral("Cannot create session: No token or previous failure."));
    return;
  }

  QNetworkRequest request = createRequest(QStringLiteral("/sessions"));
  QJsonObject json;
  json[QStringLiteral("prompt")] =
      requestData.value(QStringLiteral("prompt")).toString();

  QJsonObject sourceContext;
  QString sourceStr = requestData.value(QStringLiteral("source")).toString();
  if (!sourceStr.startsWith(QStringLiteral("sources/")) &&
      !sourceStr.isEmpty()) {
    sourceStr = QStringLiteral("sources/") + sourceStr;
  }
  sourceContext[QStringLiteral("source")] = sourceStr;

  if (sourceStr.startsWith(QStringLiteral("sources/github/"))) {
    QJsonObject githubRepoContext;
    if (requestData.contains(QStringLiteral("startingBranch"))) {
      githubRepoContext[QStringLiteral("startingBranch")] =
          requestData.value(QStringLiteral("startingBranch")).toString();
    } else {
      githubRepoContext[QStringLiteral("startingBranch")] =
          QStringLiteral("main");
    }
    sourceContext[QStringLiteral("githubRepoContext")] = githubRepoContext;
  }

  json[QStringLiteral("sourceContext")] = sourceContext;

  if (requestData.contains(QStringLiteral("requirePlanApproval"))) {
    json[QStringLiteral("requirePlanApproval")] =
        requestData.value(QStringLiteral("requirePlanApproval")).toBool();
  }

  if (requestData.contains(QStringLiteral("automationMode"))) {
    json[QStringLiteral("automationMode")] =
        requestData.value(QStringLiteral("automationMode")).toString();
  }

  QByteArray data = QJsonDocument(json).toJson();
  QNetworkReply *reply = m_nam->post(request, data);

  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, request, json, data, requestData]() {
        QByteArray responseData = reply->readAll();
        if (reply->error() == QNetworkReply::NoError) {
          QJsonDocument doc = QJsonDocument::fromJson(responseData);
          QJsonObject sessionObj = doc.object();
          Q_EMIT sessionCreated(sessionObj);
          Q_EMIT logMessage(QStringLiteral("Session created successfully."));

          // Cache session locally
          QString path =
              QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
          QDir dir(path);
          if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
          }
          QFile file(path + QStringLiteral("/cached_sessions.json"));
          QJsonArray cachedSessions;
          if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            cachedSessions = doc.array();
            file.close();
          }
          cachedSessions.append(sessionObj);
          if (file.open(QIODevice::WriteOnly)) {
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            QJsonDocument writeDoc(cachedSessions);
            file.write(writeDoc.toJson());
            file.close();
          }
        } else {
          int statusCode =
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt();
          if (statusCode == 401 || statusCode == 403) {
            m_tokenFailed = true;
          }
          QString method = QStringLiteral("POST");
          QString url = reply->url().toString();
          QString httpReq =
              method + QStringLiteral(" ") + url + QStringLiteral("\n");
          const auto reqHeaders = request.rawHeaderList();
          for (const QByteArray &h : reqHeaders) {
            if (h.toLower() != "x-goog-api-key" &&
                h.toLower() != "authorization") {
              httpReq += QString::fromUtf8(h) + QStringLiteral(": ") +
                         QString::fromUtf8(request.rawHeader(h)) +
                         QStringLiteral("\n");
            } else {
              httpReq +=
                  QString::fromUtf8(h) + QStringLiteral(": [REDACTED]\n");
            }
          }
          httpReq += QStringLiteral("\n") + QString::fromUtf8(data);

          QString httpRes = QStringLiteral("HTTP %1 %2\n")
                                .arg(statusCode)
                                .arg(reply->errorString());
          const auto resHeaders = reply->rawHeaderList();
          for (const QByteArray &h : resHeaders) {
            httpRes += QString::fromUtf8(h) + QStringLiteral(": ") +
                       QString::fromUtf8(reply->rawHeader(h)) +
                       QStringLiteral("\n");
          }

          QString errorStr = reply->errorString();
          // Do not readAll() again, use responseData
          httpRes += QStringLiteral("\n") + QString::fromUtf8(responseData);

          QString httpDetails = QStringLiteral("=== Request ===\n") + httpReq +
                                QStringLiteral("\n\n=== Response ===\n") +
                                httpRes;

          QJsonDocument errDoc = QJsonDocument::fromJson(responseData);
          Q_EMIT sessionCreationFailed(requestData, errDoc.object(), errorStr,
                                       httpDetails);
          QString errorMsg = QStringLiteral("Failed to create session: ") +
                             reply->errorString();
          Q_EMIT errorOccurred(errorMsg);
          Q_EMIT errorOccurredWithResponse(errorMsg,
                                           QString::fromUtf8(responseData));
        }
        reply->deleteLater();
      });
}

void APIManager::listSessions(const QString &pageToken) {
  if (!canConnect()) {
    Q_EMIT logMessage(
        QStringLiteral("Skipping listSessions: No token or previous failure."));
    Q_EMIT sessionsRefreshFinished();
    return;
  }

  if (m_listSessionsReply) {
    return;
  }

  QString endpoint = QStringLiteral("/sessions");
  if (!pageToken.isEmpty()) {
    endpoint += QStringLiteral("?pageToken=") + pageToken;
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
      QString nextPageToken =
          obj.value(QStringLiteral("nextPageToken")).toString();

      Q_EMIT sessionsReceived(sessions, nextPageToken);

      Q_EMIT sessionsRefreshFinished();
      Q_EMIT logMessage(QStringLiteral("Sessions refreshed successfully."));
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
      Q_EMIT sessionsRefreshFinished();
    } else {
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to list sessions: ") +
                           reply->errorString());
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
    Q_EMIT errorOccurred(QStringLiteral(
        "Cannot get session details: No token or previous failure."));
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

  if (cleanId.contains(QStringLiteral("..")) ||
      cleanId.contains(QStringLiteral("/"))) {
    Q_EMIT errorOccurred(QStringLiteral("Invalid session ID."));
    return;
  }

  QString endpoint = QStringLiteral("/sessions/") + cleanId;

  QNetworkRequest request = createRequest(endpoint);
  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      Q_EMIT sessionDetailsReceived(doc.object());
    } else {
      int statusCode =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      Q_EMIT errorOccurred(QStringLiteral("Failed to get session details: ") +
                           reply->errorString());
    }
    reply->deleteLater();
  });
}

void APIManager::fetchGithubBranches(const QString &sourceId) {
  if (m_githubToken.isEmpty()) {
    return;
  }

  QString cleanId = sourceId;
  if (cleanId.startsWith(QStringLiteral("sources/github/"))) {
    cleanId = cleanId.mid(15);
  } else {
    return; // Not a github source
  }

  QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/") +
                               cleanId + QStringLiteral("/branches")));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QVariant(QStringLiteral("application/json")));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  QString auth = QStringLiteral("Bearer ") + m_githubToken;
  request.setRawHeader("Authorization", auth.toUtf8());

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isArray()) {
        Q_EMIT githubBranchesReceived(sourceId, doc.array());
      }
    }
  });
}
