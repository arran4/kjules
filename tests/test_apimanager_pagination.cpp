#include <QtTest>
#define private public
#include "../src/apimanager.h"
#undef private
#include <QNetworkAccessManager>
#include <QNetworkReply>

class MockNetworkReply : public QNetworkReply {
  Q_OBJECT
public:
  MockNetworkReply(const QByteArray &data, const QByteArray &linkHeader) : QNetworkReply() {
    content = data;
    offset = 0;
    setRawHeader("Link", linkHeader);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setOpenMode(QIODevice::ReadOnly);
  }

  void emitFinished() {
    setFinished(true);
    Q_EMIT finished();
  }

  void abort() override {}
  qint64 readData(char *data, qint64 maxlen) override {
    qint64 len = qMin(maxlen, qint64(content.size()) - offset);
    memcpy(data, content.constData() + offset, len);
    offset += len;
    return len;
  }

  bool isSequential() const override { return true; }

private:
  QByteArray content;
  qint64 offset;
};

class MockNetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT
public:
  MockNetworkAccessManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}

protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override {
    Q_UNUSED(op);
    Q_UNUSED(outgoingData);
    QString url = request.url().toString();
    QByteArray data;
    QByteArray linkHeader;

    if (url.contains(QStringLiteral("page=1")) || !url.contains(QStringLiteral("page="))) {
      data = "[{\"number\": 1, \"title\": \"Issue 1\"}, {\"number\": 2, \"title\": \"PR 2\", \"pull_request\": {}}]";
      linkHeader = "<https://api.github.com/repos/owner/repo/issues?page=2>; rel=\"next\"";
    } else if (url.contains(QStringLiteral("page=2"))) {
      data = "[{\"number\": 3, \"title\": \"Issue 3\"}]";
      linkHeader = "";
    } else {
      data = "[]";
    }

    MockNetworkReply *reply = new MockNetworkReply(data, linkHeader);
    QTimer::singleShot(0, reply, &MockNetworkReply::emitFinished);
    return reply;
  }
};

class TestAPIManagerPagination : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void testIssuesPaginationAndFiltering() {
    APIManager apiManager;
    apiManager.setGithubToken(QStringLiteral("fake-token"));

    MockNetworkAccessManager *mockNam = new MockNetworkAccessManager(&apiManager);
    delete apiManager.m_nam;
    apiManager.m_nam = mockNam;

    QSignalSpy spy(&apiManager, &APIManager::githubIssuesReceived);
    apiManager.fetchGithubIssues(QStringLiteral("src-1"), QStringLiteral("owner"), QStringLiteral("repo"));

    QVERIFY(spy.wait(1000));

    QCOMPARE(spy.count(), 1);
    QJsonArray issues = spy.first().at(1).toJsonArray();

    QCOMPARE(issues.size(), 2);
    QCOMPARE(issues.at(0).toObject().value(QStringLiteral("number")).toInt(), 1);
    QCOMPARE(issues.at(1).toObject().value(QStringLiteral("number")).toInt(), 3);
  }
};

QTEST_MAIN(TestAPIManagerPagination)
#include "test_apimanager_pagination.moc"
