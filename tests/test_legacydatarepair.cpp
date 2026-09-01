#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "../src/legacydatarepair.h"
#include "../src/queuemodel.h"
#include "../src/sessionmodel.h"

class TestLegacyDataRepair : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

  void cleanup() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QDir(legacyDir).removeRecursively();

    QString currentDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(currentDir).removeRecursively();
  }

  void writeJsonFile(const QString &path, const QJsonDocument &doc) {
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(doc.toJson());
    file.close();
  }

  void testLegacyFollowingOnly() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    legacySessions.append(sess1);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, &queueModel);

    QCOMPARE(result.legacyFollowingCount, 1);
    QCOMPARE(result.currentFollowingCount, 0);
    QCOMPARE(result.followingToRecover, 1);
    QCOMPARE(result.followingAlreadyPresent, 0);

    auto mergeResult = repair.performMerge(&sessionModel, &queueModel);
    QCOMPARE(mergeResult.error, QString());
    QCOMPARE(sessionModel.rowCount(), 1);
    QCOMPARE(sessionModel.data(sessionModel.index(0, 0), SessionModel::IdRole).toString(), QStringLiteral("sess1"));
  }

  void testFollowingOverlap() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    sess1[QStringLiteral("title")] = QStringLiteral("Legacy Title");
    QJsonObject sess2;
    sess2[QStringLiteral("id")] = QStringLiteral("sess2");
    legacySessions.append(sess1);
    legacySessions.append(sess2);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QJsonArray currentSessions;
    QJsonObject curSess1;
    curSess1[QStringLiteral("id")] = QStringLiteral("sess1");
    curSess1[QStringLiteral("title")] = QStringLiteral("Current Title");
    currentSessions.append(curSess1);
    sessionModel.setSessions(currentSessions);

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, nullptr);
    QCOMPARE(result.legacyFollowingCount, 2);
    QCOMPARE(result.currentFollowingCount, 1);
    QCOMPARE(result.followingToRecover, 1);
    QCOMPARE(result.followingAlreadyPresent, 1);

    repair.performMerge(&sessionModel, nullptr);
    QCOMPARE(sessionModel.rowCount(), 2);

    bool foundSess1 = false;
    for (int i = 0; i < sessionModel.rowCount(); i++) {
      if (sessionModel.data(sessionModel.index(i, 0), SessionModel::IdRole).toString() == QStringLiteral("sess1")) {
        QCOMPARE(sessionModel.data(sessionModel.index(i, 0), Qt::DisplayRole).toString(),
                 QStringLiteral("Current Title"));
        foundSess1 = true;
      }
    }
    QVERIFY(foundSess1);
  }

  void testFollowingEmptyIdSkipped() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral(""); // Empty ID
    QJsonObject sess2;
    sess2[QStringLiteral("id")] = QStringLiteral("sess2");
    legacySessions.append(sess1);
    legacySessions.append(sess2);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, nullptr);
    QCOMPARE(result.followingToRecover, 1); // Only sess2 should be recovered

    repair.performMerge(&sessionModel, nullptr);
    QCOMPARE(sessionModel.rowCount(), 1);
    QCOMPARE(sessionModel.data(sessionModel.index(0, 0), SessionModel::IdRole).toString(), QStringLiteral("sess2"));
  }

  void testFollowingBareArrayFormat() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    legacySessions.append(sess1);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, nullptr);
    QCOMPARE(result.legacyFollowingCount, 1);
    QCOMPARE(result.followingToRecover, 1);
  }

  void testLegacyQueueOnly() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonObject qObj;
    QJsonArray items;
    QJsonObject req1;
    req1[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("test1")}};
    req1[QStringLiteral("lastError")] = QStringLiteral("err");
    items.append(req1);
    qObj[QStringLiteral("items")] = items;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);
    QCOMPARE(result.legacyQueueCount, 1);
    QCOMPARE(result.queueToRecover, 1);

    auto mergeResult = repair.performMerge(nullptr, &queueModel);
    QCOMPARE(mergeResult.error, QString());
    QCOMPARE(queueModel.size(), 1);
    QueueItem recovered = queueModel.getItem(0);
    QCOMPARE(recovered.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("test1"));
    QCOMPARE(recovered.lastError, QStringLiteral("err"));
  }

  void testQueueOverlapAndMultiplicity() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonObject qObj;
    QJsonArray items;

    QJsonObject reqA;
    reqA[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("A")}};
    QJsonObject reqA2 = reqA; // Second copy of A
    QJsonObject reqB;
    reqB[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("B")}};
    items.append(reqA);
    items.append(reqA2);
    items.append(reqB);
    qObj[QStringLiteral("items")] = items;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));
    queueModel.enqueue(QJsonObject{{QStringLiteral("prompt"), QStringLiteral("A")}}); // Current has one A

    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);

    QCOMPARE(result.legacyQueueCount, 3);
    QCOMPARE(result.currentQueueCount, 1);
    QCOMPARE(result.queueAlreadyPresent, 1); // One A matched
    QCOMPARE(result.queueToRecover, 2);      // One A and one B recovered

    repair.performMerge(nullptr, &queueModel);
    QCOMPARE(queueModel.size(), 3); // 1 original + 2 recovered
  }

  void testQueueTimestampsIgnored() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonObject qObj;
    QJsonArray items;
    QJsonObject req1;
    req1[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("test1")}};
    items.append(req1);
    qObj[QStringLiteral("items")] = items;
    QJsonArray runTs;
    runTs.append(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    qObj[QStringLiteral("m_runTimestamps")] = runTs;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    repair.performMerge(nullptr, &queueModel);

    QCOMPARE(queueModel.size(), 1);
  }

  void testQueueBareArrayFormat() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray items;
    QJsonObject req1;
    QJsonObject innerData;
    innerData[QStringLiteral("prompt")] = QStringLiteral("test1");
    req1[QStringLiteral("requestData")] = innerData;
    items.append(req1);
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(items));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);
    QCOMPARE(result.legacyQueueCount, 1);
    QCOMPARE(result.queueToRecover, 1);
    repair.performMerge(nullptr, &queueModel);
    QCOMPARE(queueModel.size(), 1);
    QCOMPARE(queueModel.getItem(0).requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("test1"));
  }

  void testInvalidJson() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QDir().mkpath(legacyDir);

    QFile file(legacyDir + QStringLiteral("/queue.json"));
    file.open(QIODevice::WriteOnly);
    file.write("{\"invalid_json"); // malformed
    file.close();

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);

    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.error.contains(QStringLiteral("malformed")));
    QCOMPARE(result.queueToRecover, 0);
  }

  void testMalformedShapeValidation() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QDir().mkpath(legacyDir);

    // Empty object but valid JSON
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(QJsonObject()));

    // Object without requested keys
    QJsonObject wrongQueue;
    wrongQueue[QStringLiteral("items")] = QStringLiteral("not-an-array");
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(wrongQueue));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, &queueModel);

    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.error.contains(QStringLiteral("Following")));
    QVERIFY(result.error.contains(QStringLiteral("Queue")));
    QCOMPARE(result.followingToRecover, 0);
    QCOMPARE(result.queueToRecover, 0);
  }

  void testMixedArraysWithInvalidEntries() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");

    // Invalid ID in following
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1"); // valid
    QJsonObject sess2;                                     // missing id
    QJsonObject sess3;
    sess3[QStringLiteral("id")] = QStringLiteral(""); // empty id
    legacySessions.append(sess1);
    legacySessions.append(sess2);
    legacySessions.append(sess3);
    legacySessions.append(QJsonValue(42)); // not even an object
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    // Invalid queue entries
    QJsonArray items;
    QJsonObject req1;
    req1[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("valid")}}; // valid
    QJsonObject req2; // missing requestData
    QJsonObject req3;
    req3[QStringLiteral("requestData")] = QJsonObject(); // empty requestData
    items.append(req1);
    items.append(req2);
    items.append(req3);
    items.append(QJsonValue(false)); // not an object
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(items));

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, &queueModel);

    QCOMPARE(result.followingToRecover, 1);
    QCOMPARE(result.followingSkippedInvalid, 3);
    QCOMPARE(result.queueToRecover, 1);
    QCOMPARE(result.queueSkippedInvalid, 3);
  }

  void testMissingLegacyFiles() {
    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));
    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);

    QCOMPARE(result.error, QString());
    QCOMPARE(result.legacyQueueCount, 0);
    QCOMPARE(result.queueToRecover, 0);
  }

  void testIdempotence() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonObject qObj;
    QJsonArray items;
    QJsonObject req1;
    req1[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("test1")}};
    items.append(req1);
    qObj[QStringLiteral("items")] = items;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    repair.performMerge(nullptr, &queueModel);
    QCOMPARE(queueModel.size(), 1);

    auto result2 = repair.analyze(nullptr, &queueModel);
    QCOMPARE(result2.queueToRecover, 0); // No new items
  }

  void testBackupFailureMock() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    legacySessions.append(sess1);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    QString currentDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(currentDir);

    // Provide a file to back up, so the backup logic is triggered
    QJsonArray items;
    QJsonObject curReq;
    curReq[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("current1")}};
    items.append(curReq);
    QJsonObject qObj;
    qObj[QStringLiteral("items")] = items;
    writeJsonFile(currentDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    // Also populate a legacy file so there is work to do
    QJsonArray legacyItems;
    QJsonObject legReq;
    legReq[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("legacy1")}};
    legacyItems.append(legReq);
    QJsonObject legObj;
    legObj[QStringLiteral("items")] = legacyItems;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(legObj));

    QueueModel queueModel(nullptr, QStringLiteral("queue.json"));

    QFile block(currentDir + QStringLiteral("/repair-backups"));
    block.open(QIODevice::WriteOnly);
    block.write("blocked");
    block.close();

    LegacyDataRepair repair;
    auto result = repair.performMerge(nullptr, &queueModel);

    QVERIFY(!result.fatalError.isEmpty());
    QCOMPARE(queueModel.size(), 1);

    block.remove();
  }

  void testValidOneComponentRecovery() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonArray legacySessions;
    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    legacySessions.append(sess1);
    writeJsonFile(legacyDir + QStringLiteral("/cached_all_sessions.json"), QJsonDocument(legacySessions));

    QFile file(legacyDir + QStringLiteral("/queue.json"));
    file.open(QIODevice::WriteOnly);
    file.write("{\"invalid_json"); // malformed
    file.close();

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));

    LegacyDataRepair repair;
    auto result = repair.analyze(&sessionModel, &queueModel);

    QVERIFY(!result.error.isEmpty());
    QCOMPARE(result.followingToRecover, 1);
    QCOMPARE(result.queueToRecover, 0);

    repair.performMerge(&sessionModel, &queueModel);
    QCOMPARE(sessionModel.rowCount(), 1);
    QCOMPARE(queueModel.size(), 0);
  }

  void testReverseQueueMultiplicity() {
    QString legacyDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
    QJsonObject qObj;
    QJsonArray items;
    QJsonObject reqA;
    reqA[QStringLiteral("requestData")] = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("A")}};
    items.append(reqA);
    qObj[QStringLiteral("items")] = items;
    writeJsonFile(legacyDir + QStringLiteral("/queue.json"), QJsonDocument(qObj));

    QueueModel queueModel(nullptr, QStringLiteral("test_current_queue.json"));
    queueModel.enqueue(QJsonObject{{QStringLiteral("prompt"), QStringLiteral("A")}});
    queueModel.enqueue(QJsonObject{{QStringLiteral("prompt"), QStringLiteral("A")}});

    LegacyDataRepair repair;
    auto result = repair.analyze(nullptr, &queueModel);
    QCOMPARE(result.queueToRecover, 0);
  }
};

QTEST_MAIN(TestLegacyDataRepair)
#include "test_legacydatarepair.moc"
