#include "../src/api/apierror.h"
#include <QNetworkReply>
#include <QtTest>

class TestApiError : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void testBasicError();
  void testApiErrorDetector();
};

class MockNetworkReply : public QNetworkReply {
  Q_OBJECT
public:
  MockNetworkReply(int statusCode, const QByteArray &body = QByteArray(), QObject *parent = nullptr)
      : QNetworkReply(parent), m_body(body) {
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
    setFinished(true);
  }

  void addRawHeader(const QByteArray &headerName, const QByteArray &value) { setRawHeader(headerName, value); }

  void setReplyError(NetworkError errorCode, const QString &errorString) { setError(errorCode, errorString); }

  void abort() override {}
  qint64 bytesAvailable() const override { return m_body.size(); }
  bool isSequential() const override { return true; }
  qint64 readData(char *data, qint64 maxlen) override {
    qint64 len = qMin(maxlen, static_cast<qint64>(m_body.size()));
    memcpy(data, m_body.constData(), len);
    m_body.remove(0, len);
    return len;
  }

private:
  QByteArray m_body;
};

void TestApiError::testBasicError() {
  // Basic test to verify it compiles and runs.
  ApiError error(ApiError::Type::Network, QStringLiteral("Test Error"), 500);
  QCOMPARE(error.type(), ApiError::Type::Network);
  QCOMPARE(error.message(), QStringLiteral("Test Error"));
  QCOMPARE(error.httpStatusCode(), 500);
}

void TestApiError::testApiErrorDetector() {
  {
    MockNetworkReply reply(401);
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::Authentication);
  }
  {
    MockNetworkReply reply(403);
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::PermissionDenied);
  }
  {
    MockNetworkReply reply(403);
    reply.addRawHeader("x-ratelimit-remaining", "0");
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::RateLimit);
  }
  {
    MockNetworkReply reply(429);
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::RateLimit);
  }
  {
    MockNetworkReply reply(403);
    reply.addRawHeader("retry-after", "60");
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::RateLimit);
  }
  {
    QByteArray body = "{\"error\": {\"status\": \"RESOURCE_EXHAUSTED\"}}";
    MockNetworkReply reply(403, body);
    ApiError err = ApiErrorDetector::detect(&reply, body);
    QCOMPARE(err.type(), ApiError::Type::RateLimit);
  }
  {
    QByteArray body = "{\"error\": {\"status\": \"INVALID_ARGUMENT\"}}";
    MockNetworkReply reply(400, body);
    ApiError err = ApiErrorDetector::detect(&reply, body);
    QCOMPARE(err.type(), ApiError::Type::Validation);
  }
  {
    MockNetworkReply reply(200);
    reply.setReplyError(QNetworkReply::ConnectionRefusedError, QStringLiteral("Connection refused"));
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::Network);
  }
  {
    MockNetworkReply reply(404);
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::NotFound);
  }
  {
    MockNetworkReply reply(500);
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::ServerError);
  }
  {
    MockNetworkReply reply(200);
    reply.setReplyError(QNetworkReply::OperationCanceledError, QStringLiteral("Canceled"));
    ApiError err = ApiErrorDetector::detect(&reply);
    QCOMPARE(err.type(), ApiError::Type::Canceled);
  }
}

QTEST_MAIN(TestApiError)
#include "test_apierror.moc"
