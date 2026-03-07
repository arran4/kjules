#include "apimanager.h"
#include <KWallet>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

const QString BASE_URL = QStringLiteral("https://jules.googleapis.com/v1alpha");

APIManager::APIManager(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)),
      m_wallet(nullptr), m_tokenFailed(false), m_listSourcesReply(nullptr), m_listSessionsReply(nullptr) {
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
  QNetworkRequest request(QUrl(BASE_URL + endpoint));
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

void APIManager::listSources(const QString &pageToken) {
  if (!canConnect()) {
    Q_EMIT logMessage(
        QStringLiteral("Skipping listSources: No token or previous failure."));
    Q_EMIT sourcesRefreshFinished();
    return;
  }

  if (m_listSourcesReply) {
    // If a request is already in progress, abort it or ignore new request.
    // For additive pagination, we might just ignore the new request if it's the
    // same, but the UI should prevent calling this if already refreshing.
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
                               const QString &automationMode) {
  if (!canConnect()) {
    Q_EMIT errorOccurred(
        QStringLiteral("Cannot create session: No token or previous failure."));
    return;
  }
  QJsonObject requestData;
  requestData[QStringLiteral("source")] = source;
  requestData[QStringLiteral("prompt")] = prompt;
  if (!automationMode.isEmpty()) {
    requestData[QStringLiteral("automationMode")] = automationMode;
  }
  createSessionAsync(requestData);
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
  sourceContext[QStringLiteral("source")] =
      requestData.value(QStringLiteral("source")).toString();

  json[QStringLiteral("sourceContext")] = sourceContext;
  if (requestData.contains(QStringLiteral("automationMode"))) {
    json[QStringLiteral("automationMode")] =
        requestData.value(QStringLiteral("automationMode")).toString();
  }

  QByteArray data = QJsonDocument(json).toJson();
  QNetworkReply *reply = m_nam->post(request, data);

  connect(reply, &QNetworkReply::finished, this, [this, reply, json]() {
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
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode == 401 || statusCode == 403) {
        m_tokenFailed = true;
      }
      QString errorStr = reply->errorString();
      QByteArray errorData = reply->readAll();
      QJsonDocument errDoc = QJsonDocument::fromJson(errorData);
      Q_EMIT sessionCreationFailed(json, errDoc.object(), errorStr);
      QString errorMsg =
          QStringLiteral("Failed to create session: ") + reply->errorString();
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
      Q_EMIT sessionsReceived(sessions);

      QString nextPageToken =
          obj.value(QStringLiteral("nextPageToken")).toString();
      if (!nextPageToken.isEmpty()) {
        listSessions(nextPageToken);
      } else {
        Q_EMIT sessionsRefreshFinished();
        Q_EMIT logMessage(QStringLiteral("Sessions refreshed successfully."));
      }
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
  // If just ID, prepend "sessions/"? API doc says name is "sessions/..."
  // We'll assume the caller passes the full name or ID correctly or we
  // construct it. The listSessions returns objects with "name": "sessions/..."
  // and "id": "..."

  // If we use the ID, we might need to construct the URL.
  // The endpoint is /sessions/{sessionId}

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
