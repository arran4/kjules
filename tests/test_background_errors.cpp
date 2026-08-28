#include "../src/apimanager.h"
#include "../src/errorsmodel.h"
#include <QDateTime>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

// We can test APIManager directly without launching MainWindow.
// "At minimum, add APIManager-level tests showing:
// - a simulated failure from reloadSession(id, true) emits the background context as true;
// - the equivalent foreground call emits false;"

#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

class Mock503NetworkReply : public QNetworkReply {
  Q_OBJECT
public:
  Mock503NetworkReply(QObject *parent = nullptr) : QNetworkReply(parent) {
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 503);
    setError(QNetworkReply::ServiceUnavailableError, QStringLiteral("Service Unavailable"));
    QTimer::singleShot(0, this, [this]() { Q_EMIT finished(); });
  }
  void abort() override {}
  qint64 readData(char *data, qint64 maxlen) override {
    Q_UNUSED(data);
    Q_UNUSED(maxlen);
    return -1;
  }
};

class Mock503NetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT
public:
  Mock503NetworkAccessManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}

protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override {
    Q_UNUSED(op);
    Q_UNUSED(request);
    Q_UNUSED(outgoingData);
    return new Mock503NetworkReply(this);
  }
};

class Mock200EmptyJsonNetworkReply : public QNetworkReply {
  Q_OBJECT
  QByteArray m_data;

public:
  Mock200EmptyJsonNetworkReply(const QByteArray &data, QObject *parent = nullptr)
      : QNetworkReply(parent), m_data(data) {
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    QTimer::singleShot(0, this, [this]() {
      setOpenMode(QIODevice::ReadOnly);
      Q_EMIT readyRead();
      Q_EMIT finished();
    });
  }
  void abort() override {}
  qint64 readData(char *data, qint64 maxlen) override {
    qint64 len = qMin(maxlen, (qint64)m_data.size());
    if (len > 0) {
      memcpy(data, m_data.constData(), len);
      m_data.remove(0, len);
      return len;
    }
    return 0;
  }
};

class Mock200EmptyJsonNetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT
  QByteArray m_responseData;

public:
  Mock200EmptyJsonNetworkAccessManager(const QByteArray &responseData, QObject *parent = nullptr)
      : QNetworkAccessManager(parent), m_responseData(responseData) {}

protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override {
    Q_UNUSED(op);
    Q_UNUSED(request);
    Q_UNUSED(outgoingData);
    return new Mock200EmptyJsonNetworkReply(m_responseData, this);
  }
};

class TestBackgroundErrors : public QObject {
  Q_OBJECT

private Q_SLOTS:

  void testReloadSessionNormalizedIdSuccess() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray validResponse = "{\"id\": \"foo\", \"state\": \"COMPLETED\"}";
    Mock200EmptyJsonNetworkAccessManager *mockNam1 =
        new Mock200EmptyJsonNetworkAccessManager(validResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam1;

    QSignalSpy reloadedSpy1(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy1(&apiManager, &APIManager::sessionReloadFailed);

    // request foo, response foo
    apiManager.reloadSession(QStringLiteral("foo"), false);
    QVERIFY(reloadedSpy1.wait(1000));
    QCOMPARE(reloadedSpy1.count(), 1);
    QCOMPARE(failedSpy1.count(), 0);

    // request sessions/foo, response foo
    Mock200EmptyJsonNetworkAccessManager *mockNam2 =
        new Mock200EmptyJsonNetworkAccessManager(validResponse, &apiManager);
    apiManager.m_nam = mockNam2;
    QSignalSpy reloadedSpy2(&apiManager, &APIManager::sessionReloaded);
    apiManager.reloadSession(QStringLiteral("sessions/foo"), false);
    QVERIFY(reloadedSpy2.wait(1000));
    QCOMPARE(reloadedSpy2.count(), 1);

    // request /sessions/foo, response foo
    Mock200EmptyJsonNetworkAccessManager *mockNam3 =
        new Mock200EmptyJsonNetworkAccessManager(validResponse, &apiManager);
    apiManager.m_nam = mockNam3;
    QSignalSpy reloadedSpy3(&apiManager, &APIManager::sessionReloaded);
    apiManager.reloadSession(QStringLiteral("/sessions/foo"), false);
    QVERIFY(reloadedSpy3.wait(1000));
    QCOMPARE(reloadedSpy3.count(), 1);

    // request session/foo, response foo (since cleanSessionId does session/foo)
    Mock200EmptyJsonNetworkAccessManager *mockNam4 =
        new Mock200EmptyJsonNetworkAccessManager(validResponse, &apiManager);
    apiManager.m_nam = mockNam4;
    QSignalSpy reloadedSpy4(&apiManager, &APIManager::sessionReloaded);
    apiManager.reloadSession(QStringLiteral("session/foo"), false);
    QVERIFY(reloadedSpy4.wait(1000));
    QCOMPARE(reloadedSpy4.count(), 1);
  }

  void testReloadSessionEmptyIdFailed() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray emptyIdResponse = "{\"id\": \"\", \"state\": \"COMPLETED\"}";
    Mock200EmptyJsonNetworkAccessManager *mockNam =
        new Mock200EmptyJsonNetworkAccessManager(emptyIdResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy reloadedSpy(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy(&apiManager, &APIManager::sessionReloadFailed);

    apiManager.reloadSession(QStringLiteral("requested_id"), false);
    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(reloadedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);

    QVariantList args = failedSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("requested_id"));
  }

  void testReloadSessionMissingIdFailed() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray missingIdResponse = "{\"state\": \"COMPLETED\"}";
    Mock200EmptyJsonNetworkAccessManager *mockNam =
        new Mock200EmptyJsonNetworkAccessManager(missingIdResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy reloadedSpy(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy(&apiManager, &APIManager::sessionReloadFailed);

    apiManager.reloadSession(QStringLiteral("requested_id"), false);
    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(reloadedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVariantList args = failedSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("requested_id"));
  }

  void testReloadSessionWrongIdFailed() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray wrongIdResponse = "{\"id\": \"wrong_id\", \"state\": \"COMPLETED\"}";
    Mock200EmptyJsonNetworkAccessManager *mockNam =
        new Mock200EmptyJsonNetworkAccessManager(wrongIdResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy reloadedSpy(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy(&apiManager, &APIManager::sessionReloadFailed);

    apiManager.reloadSession(QStringLiteral("requested_id"), false);
    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(reloadedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVariantList args = failedSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("requested_id"));
  }

  void testReloadSessionMalformedJsonFailed() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray malformedJsonResponse = "this is not json";
    Mock200EmptyJsonNetworkAccessManager *mockNam =
        new Mock200EmptyJsonNetworkAccessManager(malformedJsonResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy reloadedSpy(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy(&apiManager, &APIManager::sessionReloadFailed);

    apiManager.reloadSession(QStringLiteral("requested_id"), false);
    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(reloadedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVariantList args = failedSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("requested_id"));
  }

  void testReloadSessionNonObjectJsonFailed() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key"));
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    QByteArray arrayJsonResponse = "[{\"id\": \"requested_id\"}]";
    Mock200EmptyJsonNetworkAccessManager *mockNam =
        new Mock200EmptyJsonNetworkAccessManager(arrayJsonResponse, &apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy reloadedSpy(&apiManager, &APIManager::sessionReloaded);
    QSignalSpy failedSpy(&apiManager, &APIManager::sessionReloadFailed);

    apiManager.reloadSession(QStringLiteral("requested_id"), false);
    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(reloadedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVariantList args = failedSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("requested_id"));
  }

  void testApiManagerBackgroundContextNetwork() {
    APIManager apiManager;
    apiManager.setApiKey(QStringLiteral("fake-key")); // bypass canConnect check
    apiManager.setBaseUrl(QStringLiteral("http://localhost"));

    Mock503NetworkAccessManager *mockNam = new Mock503NetworkAccessManager(&apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy errorSpy(&apiManager, &APIManager::errorOccurred);
    QSignalSpy reloadSpy(&apiManager, &APIManager::sessionReloadFailed);

    // Foreground call
    apiManager.reloadSession(QStringLiteral("valid_id"), false);
    QVERIFY(errorSpy.wait(1000));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    QVariantList errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), false);

    QVariantList reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), false);

    // Background call
    apiManager.reloadSession(QStringLiteral("valid_id"), true);
    QVERIFY(errorSpy.wait(1000));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), true);

    reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), true);
  }
  void testApiManagerBackgroundContext() {
    APIManager apiManager;

    // We intentionally don't set a valid token so it fails immediately.
    QSignalSpy errorSpy(&apiManager, &APIManager::errorOccurred);
    QSignalSpy reloadSpy(&apiManager, &APIManager::sessionReloadFailed);

    // Foreground call
    apiManager.reloadSession(QStringLiteral("invalid_id"), false);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    // Check errorOccurred signature is void errorOccurred(const QString &message, bool isBackground)
    QVariantList errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), false);

    QVariantList reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), false);

    // Background call
    apiManager.reloadSession(QStringLiteral("invalid_id"), true);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), true);

    reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), true);
  }
};

QTEST_MAIN(TestBackgroundErrors)
#include "test_background_errors.moc"
