#include "apimanager.h"
#include <QDebug>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <KWallet>

const QString BASE_URL = "https://jules.googleapis.com/v1alpha";

APIManager::APIManager(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_wallet(nullptr)
{
    loadApiKeyFromWallet();
}

APIManager::~APIManager()
{
    if (m_wallet) {
        delete m_wallet;
    }
}

void APIManager::setApiKey(const QString &key)
{
    m_apiKey = key;
    saveApiKeyToWallet(key);
}

QString APIManager::apiKey() const
{
    return m_apiKey;
}

void APIManager::setGithubToken(const QString &token)
{
    m_githubToken = token;
    saveGithubTokenToWallet(token);
}

QString APIManager::githubToken() const
{
    return m_githubToken;
}

void APIManager::loadApiKeyFromWallet()
{
    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
    if (m_wallet) {
        connect(m_wallet, &KWallet::Wallet::walletOpened, this, &APIManager::onWalletOpened);
    }
}

void APIManager::saveApiKeyToWallet(const QString &key)
{
    if (m_wallet && m_wallet->isOpen()) {
        m_wallet->writePassword("jules_api_key", key);
    }
}

void APIManager::loadGithubTokenFromWallet()
{
    if (m_wallet && m_wallet->isOpen()) {
        QString token;
        if (m_wallet->readPassword("github_token", token) == 0) {
            m_githubToken = token;
            emit logMessage("GitHub Token loaded from KWallet");
        }
    }
}

void APIManager::saveGithubTokenToWallet(const QString &token)
{
    if (m_wallet && m_wallet->isOpen()) {
        m_wallet->writePassword("github_token", token);
    }
}

void APIManager::onWalletOpened(bool success)
{
    if (success && m_wallet) {
        if (!m_wallet->hasFolder("kjules")) {
            m_wallet->createFolder("kjules");
        }
        m_wallet->setFolder("kjules");
        QString key;
        if (m_wallet->readPassword("jules_api_key", key) == 0) {
            m_apiKey = key;
            emit logMessage("API Key loaded from KWallet");
        }
        QString token;
        if (m_wallet->readPassword("github_token", token) == 0) {
            m_githubToken = token;
            emit logMessage("GitHub Token loaded from KWallet");
        }
    } else {
        emit errorOccurred("Failed to open KWallet");
    }
}

QNetworkRequest APIManager::createRequest(const QString &endpoint, const QString &overrideApiKey)
{
    QNetworkRequest request(QUrl(BASE_URL + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString key = overrideApiKey.isEmpty() ? m_apiKey : overrideApiKey;
    if (!key.isEmpty()) {
        request.setRawHeader("X-Goog-Api-Key", key.toUtf8());
    }
    return request;
}

void APIManager::testConnection(const QString &apiKey)
{
    QNetworkRequest request = createRequest("/sources", apiKey);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.object().contains("sources")) {
                 emit connectionTested(true, "Connection successful.");
            } else {
                 emit connectionTested(false, "Connection successful but no sources found or invalid response.");
            }
        } else {
            emit connectionTested(false, "Connection failed: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void APIManager::listSources()
{
    QNetworkRequest request = createRequest("/sources");
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonArray sources = doc.object().value("sources").toArray();
            emit sourcesReceived(sources);
            emit logMessage("Sources refreshed successfully.");
        } else {
            emit errorOccurred("Failed to list sources: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void APIManager::createSession(const QString &source, const QString &prompt, const QString &automationMode)
{
    QNetworkRequest request = createRequest("/sessions");
    QJsonObject json;
    json["prompt"] = prompt;

    QJsonObject sourceContext;
    sourceContext["source"] = source;
    // Assuming github repo context is needed or derived from source
    // For simplicity, we just pass the source string as requested by API logic
    // Wait, API doc says sourceContext has source and githubRepoContext
    // The source string "sources/github/owner/repo"

    // We'll need to parse the source string if we need to fill githubRepoContext details
    // But for now let's assume the API handles it or we pass minimal

    json["sourceContext"] = sourceContext;
    if (!automationMode.isEmpty()) {
        json["automationMode"] = automationMode;
    }

    QByteArray data = QJsonDocument(json).toJson();
    QNetworkReply *reply = m_nam->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            emit sessionCreated(doc.object());
            emit logMessage("Session created successfully.");
        } else {
             emit errorOccurred("Failed to create session: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void APIManager::listSessions()
{
    QNetworkRequest request = createRequest("/sessions");
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
         if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            // The API returns a list of sessions, possibly in "sessions" key?
            // Wait, looking at API docs example response for ListSessions isn't fully shown, but it implies standard list response.
            // Usually { "sessions": [ ... ], "nextPageToken": ... }
            QJsonArray sessions;
            if (doc.object().contains("sessions")) {
                sessions = doc.object().value("sessions").toArray();
            } else {
                // If it's just an array (unlikely for google apis)
                // But let's assume "sessions" key
            }
            emit sessionsReceived(sessions);
             emit logMessage(QString("Refreshed %1 sessions.").arg(sessions.size()));
        } else {
             emit errorOccurred("Failed to list sessions: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void APIManager::getSession(const QString &sessionId)
{
    // sessionId should be the full resource name e.g. "sessions/123..."
    // If just ID, prepend "sessions/"? API doc says name is "sessions/..."
    // We'll assume the caller passes the full name or ID correctly or we construct it.
    // The listSessions returns objects with "name": "sessions/..." and "id": "..."

    // If we use the ID, we might need to construct the URL.
    // The endpoint is /sessions/{sessionId}

    QString endpoint = "/" + sessionId; // Assuming sessionId is "sessions/123"
    if (!sessionId.startsWith("sessions/")) {
        endpoint = "/sessions/" + sessionId;
    }

    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_nam->get(request);
     connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
             QByteArray data = reply->readAll();
             QJsonDocument doc = QJsonDocument::fromJson(data);
             emit sessionDetailsReceived(doc.object());
        } else {
             emit errorOccurred("Failed to get session details: " + reply->errorString());
        }
        reply->deleteLater();
    });
}
