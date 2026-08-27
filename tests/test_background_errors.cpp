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


#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QBuffer>

class Mock503NetworkReply : public QNetworkReply {
  Q_OBJECT
public:
  Mock503NetworkReply(QObject *parent = nullptr) : QNetworkReply(parent) {
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 503);
    setError(QNetworkReply::ServiceUnavailableError, QStringLiteral("Service Unavailable"));
    QTimer::singleShot(0, this, [this]() {
      Q_EMIT finished();
    });
  }
  void abort() override {}
  qint64 readData(char *data, qint64 maxlen) override { Q_UNUSED(data); Q_UNUSED(maxlen); return -1; }
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

class TestBackgroundErrors : public QObject {
  Q_OBJECT

private Q_SLOTS:

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
