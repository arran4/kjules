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

#include "legacydatarepair.h"
#include "queuemodel.h"
#include "sessionmodel.h"

class TestLegacyDataRepair : public QObject {
  Q_OBJECT
private slots:
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
    req1[QStringLiteral("prompt")] = QStringLiteral("test1");
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

    SessionModel sessionModel(QStringLiteral("test_current_sessions.json"));
    QFile block(currentDir + QStringLiteral("/repair-backups"));
    block.open(QIODevice::WriteOnly);
    block.write("blocked");
    block.close();

    QJsonArray emptyArr;
    sessionModel.setSessions(emptyArr);

    LegacyDataRepair repair;
    auto result = repair.performMerge(&sessionModel, nullptr);

    QVERIFY(!result.error.isEmpty());
    QCOMPARE(sessionModel.rowCount(), 0);

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
