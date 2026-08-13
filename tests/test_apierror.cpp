#include <QtTest>
#include "../src/api/apierror.h"
#include <QNetworkReply>

class TestApiError : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void testBasicError();
};

void TestApiError::testBasicError() {
    // Basic test to verify it compiles and runs.
    ApiError error(ApiError::Type::Network, QStringLiteral("Test Error"), 500);
    QCOMPARE(error.type(), ApiError::Type::Network);
    QCOMPARE(error.message(), QStringLiteral("Test Error"));
    QCOMPARE(error.httpStatusCode(), 500);
}

QTEST_MAIN(TestApiError)
#include "test_apierror.moc"
