#include "../src/jobdata.h"
#include "../src/jobpolicy.h"
#include "../src/jobstore.h"
#include "../src/legacyconverter.h"
#include "../src/migrationorchestrator.h"
#include <QObject>
#include <QStandardPaths>
#include <QTest>
#include <QUuid>

class TestJob : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void testJobDataRoundTrip() {
    JobData job;
    job.id = QStringLiteral("test-id");
    job.source = QStringLiteral("source");
    job.prompt = QStringLiteral("prompt");
    job.planApproval = true;
    job.createdAt = QDateTime::currentDateTimeUtc();

    JobAttemptData attempt;
    attempt.id = QStringLiteral("attempt-id");
    attempt.julesState = QStringLiteral("IN_PROGRESS");
    job.attempts.append(attempt);

    QJsonObject json = job.toJson();
    JobData recovered = JobData::fromJson(json);

    QCOMPARE(recovered.id, job.id);
    QCOMPARE(recovered.source, job.source);
    QCOMPARE(recovered.prompt, job.prompt);
    QCOMPARE(recovered.planApproval, job.planApproval);
    QCOMPARE(recovered.attempts.size(), 1);
    QCOMPARE(recovered.attempts[0].id, attempt.id);
    QCOMPARE(recovered.attempts[0].julesState, attempt.julesState);
  }

  void testJobDataZeroOneMultipleAttemptsRoundTrip() {
    JobData zero;
    zero.id = QStringLiteral("zero");
    QJsonObject jZero = zero.toJson();
    JobData rZero = JobData::fromJson(jZero);
    QCOMPARE(rZero.id, QStringLiteral("zero"));
    QCOMPARE(rZero.attempts.size(), 0);

    JobData one;
    one.id = QStringLiteral("one");
    JobAttemptData a1;
    a1.id = QStringLiteral("a1");
    one.attempts.append(a1);
    QJsonObject jOne = one.toJson();
    JobData rOne = JobData::fromJson(jOne);
    QCOMPARE(rOne.attempts.size(), 1);

    JobData multi;
    multi.id = QStringLiteral("multi");
    JobAttemptData a2;
    a2.id = QStringLiteral("a2");
    QJsonObject req2;
    req2[QStringLiteral("snap")] = 2;
    a2.requestSnapshot = req2;
    JobAttemptData a3;
    a3.id = QStringLiteral("a3");
    QJsonObject req3;
    req3[QStringLiteral("snap")] = 3;
    a3.requestSnapshot = req3;
    multi.attempts.append(a2);
    multi.attempts.append(a3);
    QJsonObject jMulti = multi.toJson();
    JobData rMulti = JobData::fromJson(jMulti);
    QCOMPARE(rMulti.attempts.size(), 2);
    QCOMPARE(rMulti.attempts[0].id, QStringLiteral("a2"));
    QCOMPARE(rMulti.attempts[0].requestSnapshot[QStringLiteral("snap")].toInt(), 2);
    QCOMPARE(rMulti.attempts[1].id, QStringLiteral("a3"));
    QCOMPARE(rMulti.attempts[1].requestSnapshot[QStringLiteral("snap")].toInt(), 3);
  }

  void testFailedLaunchNoSessionId() {
    LegacyData data;
    QJsonObject error;
    QJsonObject req;
    req[QStringLiteral("prompt")] = QStringLiteral("p");
    error[QStringLiteral("request")] = req;
    error[QStringLiteral("message")] = QStringLiteral("work error");
    data.errors.append(error);

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());
    QCOMPARE(res.jobs.size(), 1);
    QCOMPARE(res.jobs[0].attempts.size(), 1);
    QCOMPARE(res.jobs[0].attempts[0].julesSessionId, QString()); // No remote session ID
    QCOMPARE(res.jobs[0].attempts[0].dispatchState, QStringLiteral("FAILED"));
  }

  void testFutureSchemaRejection() {
    JobStore store(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
    QFile f(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
    f.open(QIODevice::WriteOnly);
    f.write("{\"schemaVersion\": 9999, \"jobs\": []}");
    f.close();
    QVERIFY(!store.load());
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
  }

  void testFailedLoadPreservesState() {
    JobStore store(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
    JobData init;
    init.id = QStringLiteral("init");
    store.addJob(init);

    QFile f(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
    f.open(QIODevice::WriteOnly);
    f.write("{\"schemaVersion\": 1, \"jobs\": [{\"id\": \"123\", \"priority\": \"not-a-number\"}]}");
    f.close();

    QVERIFY(!store.load()); // fails

    QCOMPARE(store.jobs().size(), 1); // preserved!
    QCOMPARE(store.jobs()[0].id, QStringLiteral("init"));
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
  }

  void testJobPolicyEmptyAttemptNotViable() {
    JobData job;
    JobAttemptData attempt; // completely empty, default
    job.attempts.append(attempt);
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), false);
    QCOMPARE(JobPolicy::consumesConcurrency(attempt), false);
  }

  void testJobPolicyAcceptedFailed() {
    JobData job;
    JobAttemptData attempt;
    attempt.id = QStringLiteral("a1");
    attempt.julesState = QStringLiteral("ERROR");
    job.attempts.append(attempt);
    job.acceptedAttemptId = QStringLiteral("a1"); // invalid accepted
    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), false);
  }

  void testJobPolicyFailedAndActive() {
    JobData job;
    JobAttemptData a1;
    a1.id = QStringLiteral("a1");
    a1.julesState = QStringLiteral("ERROR");
    JobAttemptData a2;
    a2.id = QStringLiteral("a2");
    a2.julesState = QStringLiteral("IN_PROGRESS");
    job.attempts.append(a1);
    job.attempts.append(a2);

    QCOMPARE(JobPolicy::needsAttention(job), false); // Active attempt overrides attention
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), true);
    QCOMPARE(JobPolicy::shouldRemainInFollowing(job), true);
  }

  void testJobPolicyNoAttempts() {
    JobData job;
    QCOMPARE(JobPolicy::consumesConcurrency(JobAttemptData()), false);
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), false);
    QCOMPARE(JobPolicy::needsAttention(job), false);
    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), false);
    QCOMPARE(JobPolicy::shouldRemainInFollowing(job), false);
    QCOMPARE(JobPolicy::hasOutstandingAttempts(job), false);
  }

  void testJobPolicyFailedAttempt() {
    JobData job;
    JobAttemptData attempt;
    attempt.id = QStringLiteral("a1");
    attempt.dispatchState = QStringLiteral("FAILED");
    job.attempts.append(attempt);

    QCOMPARE(JobPolicy::consumesConcurrency(attempt), false);
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), false);
    QCOMPARE(JobPolicy::needsAttention(job), true);
    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), false);
    QCOMPARE(JobPolicy::shouldRemainInFollowing(job), true);
    QCOMPARE(JobPolicy::hasOutstandingAttempts(job), false);
  }

  void testJobPolicyTerminalJulesState() {
    JobData job;
    JobAttemptData attempt;
    attempt.id = QStringLiteral("a1");
    attempt.julesState = QStringLiteral("ERROR");
    job.attempts.append(attempt);

    QCOMPARE(JobPolicy::consumesConcurrency(attempt), false);
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), false);
    QCOMPARE(JobPolicy::needsAttention(job), true);
    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), false);
    QCOMPARE(JobPolicy::shouldRemainInFollowing(job), true);
  }

  void testJobPolicyCompleted() {
    JobData job;
    JobAttemptData attempt;
    attempt.id = QStringLiteral("a1");
    attempt.julesState = QStringLiteral("COMPLETED");
    job.attempts.append(attempt);

    QCOMPARE(JobPolicy::consumesConcurrency(attempt), false);
    QCOMPARE(JobPolicy::hasViableActiveAttempt(job), false);
    QCOMPARE(JobPolicy::needsAttention(job), false);
    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), true);
    QCOMPARE(JobPolicy::hasOutstandingAttempts(job), false);
  }

  void testJobPolicyWinnerOutstandingSibling() {
    JobData job;
    JobAttemptData winner;
    winner.id = QStringLiteral("w1");
    winner.julesState = QStringLiteral("COMPLETED");

    JobAttemptData sibling;
    sibling.id = QStringLiteral("s1");
    sibling.julesState = QStringLiteral("IN_PROGRESS");

    job.attempts.append(winner);
    job.attempts.append(sibling);
    job.acceptedAttemptId = QStringLiteral("w1");

    QCOMPARE(JobPolicy::isSuccessfullyComplete(job), true);
    QCOMPARE(JobPolicy::hasOutstandingAttempts(job), true);
  }

  void testJobStorePersistence() {
    JobData job;
    job.id = QStringLiteral("test-store-id");
    job.source = QStringLiteral("test-store-source");

    JobStore store(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/test_jobs.json"));
    store.addJob(job);
    QVERIFY(store.save());

    JobStore loadedStore(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                         QStringLiteral("/test_jobs.json"));
    QVERIFY(loadedStore.load());
    QCOMPARE(loadedStore.jobs().size(), 1);
    QCOMPARE(loadedStore.jobs()[0].id, job.id);

    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/test_jobs.json"));
  }

  void testIdempotentReorderingFull() {
    QueueItem item1;
    item1.requestData[QStringLiteral("id")] = QStringLiteral("req-1");
    QueueItem item2;
    item2.requestData[QStringLiteral("id")] = QStringLiteral("req-2");

    QueueItem item3; // Same request as item1, different blocking state
    item3.requestData[QStringLiteral("id")] = QStringLiteral("req-1");
    item3.isBlocked = true;
    QJsonObject bm;
    bm[QStringLiteral("reason")] = QStringLiteral("ci");
    item3.blockMetadata = bm;

    LegacyData data1;
    data1.queueItems = {item1, item2, item3};
    LegacyData data2;
    data2.queueItems = {item3, item1, item2}; // Reordered

    QDateTime ts = QDateTime::currentDateTimeUtc();
    auto res1 = LegacyConverter::convertAll(data1, ts);
    auto res2 = LegacyConverter::convertAll(data2, ts);

    QCOMPARE(res1.jobs.size(), 3);
    QCOMPARE(res2.jobs.size(), 3);

    QJsonArray arr1;
    for (auto &j : res1.jobs)
      arr1.append(j.toJson());
    QJsonArray arr2;
    for (auto &j : res2.jobs)
      arr2.append(j.toJson());

    // Check full output is identical after sorting
    auto sortArray = [](QJsonArray arr) {
      QStringList strList;
      for (const auto &v : arr)
        strList.append(QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)));
      strList.sort();
      return strList;
    };

    QCOMPARE(sortArray(arr1), sortArray(arr2));
  }

  void testMigrationSeam() {
    QString tempPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/seam_test.json");

    // Empty data trivially succeeds
    LegacyData emptyData;
    QVERIFY(MigrationOrchestrator::safeMigrationSeam(emptyData, tempPath, QDateTime::currentDateTimeUtc()));

    // Create a sentinel destination to prove failure paths are non-destructive
    QFile sf(tempPath);
    sf.open(QIODevice::WriteOnly);
    sf.write("sentinel");
    sf.close();

    // Data with operational diagnostics fails migration seam because they aren't stored
    LegacyData dataWithErrors;
    QJsonObject opError;
    QJsonObject opReq;
    opReq[QStringLiteral("prompt")] = QStringLiteral("p"); // Add prompt to prove diagnostic overrides it
    opError[QStringLiteral("request")] = opReq;
    opError[QStringLiteral("message")] = QStringLiteral("op error");
    opError[QStringLiteral("operation")] = QStringLiteral("clone");
    opError[QStringLiteral("provider")] = QStringLiteral("github"); // ensure it is classified as unattached diagnostic
    dataWithErrors.errors.append(opError);
    QVERIFY(!MigrationOrchestrator::safeMigrationSeam(dataWithErrors, tempPath, QDateTime::currentDateTimeUtc()));

    // Sentinel must be untouched!
    QFile rf(tempPath);
    rf.open(QIODevice::ReadOnly);
    QCOMPARE(rf.readAll(), QByteArray("sentinel"));
    rf.close();

    // Data without unattached errors succeeds and validates full round trip
    LegacyData data;
    QueueItem item1;
    item1.requestData[QStringLiteral("id")] = QStringLiteral("req-1");
    data.queueItems.append(item1);
    QVERIFY(MigrationOrchestrator::safeMigrationSeam(data, tempPath, QDateTime::currentDateTimeUtc()));

    // Sentinel is now replaced by valid JSON
    rf.open(QIODevice::ReadOnly);
    QVERIFY(rf.readAll() != QByteArray("sentinel"));
    rf.close();

    // Prove write failure to final path leaves existing file untouched
    // We cannot easily mock QSaveFile failure inside the same process without OS tools, but we test the unwritable
    // directory fails
    QVERIFY(!MigrationOrchestrator::safeMigrationSeam(data, QStringLiteral("/dev/null/foo.json"),
                                                      QDateTime::currentDateTimeUtc()));

    QFile::remove(tempPath);
  }

  void testLegacyFallbackSemantics() {
    LegacyData data;
    QJsonObject sess;

    sess[QStringLiteral("prompt")] = QStringLiteral("outer-prompt");
    sess[QStringLiteral("automationMode")] = QStringLiteral("outer-auto");
    sess[QStringLiteral("source")] = QStringLiteral("outer-source");
    sess[QStringLiteral("startingBranch")] = QStringLiteral("outer-branch");

    QJsonObject req;
    // Omit prompt to prove it doesn't fall back to outer when request exists
    req[QStringLiteral("automationMode")] = QStringLiteral("inner-auto");

    // Provide empty source, it should fall back to nested sourceContext, not outer
    req[QStringLiteral("source")] = QStringLiteral("");
    QJsonObject sCtx;
    sCtx[QStringLiteral("source")] = QStringLiteral("inner-source");

    QJsonObject ghCtx;
    ghCtx[QStringLiteral("startingBranch")] = QStringLiteral("inner-branch");
    sCtx[QStringLiteral("githubRepoContext")] = ghCtx;
    req[QStringLiteral("sourceContext")] = sCtx;

    sess[QStringLiteral("request")] = req;
    data.activeSessions.append(sess);

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());
    QCOMPARE(res.jobs.size(), 1);
    QCOMPARE(res.jobs[0].prompt, QStringLiteral(""));
    QCOMPARE(res.jobs[0].automationMode, QStringLiteral("inner-auto"));
    QCOMPARE(res.jobs[0].source, QStringLiteral("inner-source"));
    QCOMPARE(res.jobs[0].startingBranch, QStringLiteral("inner-branch"));
  }

  void testActiveArchiveProvenanceNoCollision() {
    LegacyData data;
    QJsonObject sess;
    sess[QStringLiteral("id")] = QStringLiteral("sess1");

    // Add identical session to BOTH active and archive
    data.activeSessions.append(sess);
    data.archivedSessions.append(sess);

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());
    QCOMPARE(res.jobs.size(), 2);
    QVERIFY(res.jobs[0].id != res.jobs[1].id);
    QVERIFY(res.jobs[0].attempts[0].id != res.jobs[1].attempts[0].id);
  }

  void testFixtures() {
    LegacyData data;

    auto loadArray = [](const QString &name) {
      QFile f(QStringLiteral(TEST_DATA_DIR "/") + name);
      f.open(QIODevice::ReadOnly);
      return QJsonDocument::fromJson(f.readAll()).array();
    };

    QJsonArray qArr = loadArray(QStringLiteral("queue.json"));
    for (const QJsonValue &v : qArr) {
      QJsonObject o = v.toObject();
      QueueItem item;
      item.requestData = o[QStringLiteral("requestData")].toObject();
      item.isBlocked = o[QStringLiteral("isBlocked")].toBool();
      item.blockMetadata = o[QStringLiteral("blockMetadata")].toObject(); // already fixed earlier but just in case
      item.errorCount = o[QStringLiteral("errorCount")].toInt();
      item.lastError = o[QStringLiteral("lastError")].toString();
      item.lastResponse = o[QStringLiteral("lastResponse")].toString();
      if (o.contains(QStringLiteral("lastTry")))
        item.lastTry = QDateTime::fromString(o[QStringLiteral("lastTry")].toString(), Qt::ISODate);
      item.pastErrors = o[QStringLiteral("pastErrors")].toArray();
      data.queueItems.append(item);
    }

    QJsonArray hArr = loadArray(QStringLiteral("holding.json"));
    for (const QJsonValue &v : hArr) {
      QJsonObject o = v.toObject();
      QueueItem item;
      item.requestData = o[QStringLiteral("requestData")].toObject();
      item.isBlocked = o[QStringLiteral("isBlocked")].toBool();
      item.errorCount = o[QStringLiteral("errorCount")].toInt();
      data.holdingItems.append(item);
    }

    data.activeSessions = loadArray(QStringLiteral("active.json"));
    data.archivedSessions = loadArray(QStringLiteral("archive.json"));
    data.errors = loadArray(QStringLiteral("errors.json"));

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());

    QCOMPARE(res.jobs.size(), 5);             // 1 queue, 1 holding, 1 active, 1 archive, 1 logical work error
    QCOMPARE(res.unattachedErrors.size(), 1); // 1 operational diagnostic

    int linkedErrors = 0;
    for (const auto &j : res.jobs) {
      if (j.id.contains(QLatin1String("queue"))) {
        QCOMPARE(j.priority, 5);
        QCOMPARE(j.planApproval, true);
      }
      for (const auto &a : j.attempts) {
        if (a.julesSessionId == QStringLiteral("sess-active")) {
          linkedErrors += a.launchErrors.size();
          QVERIFY(!a.prMetadata.isEmpty());
          QCOMPARE(a.prMetadata[QStringLiteral("status")].toString(), QStringLiteral("open"));
        } else if (a.julesSessionId == QStringLiteral("sess-archive")) {
          QVERIFY(!a.prMetadata.isEmpty());
          QCOMPARE(a.prMetadata[QStringLiteral("url")].toString(), QStringLiteral("https://github.com/test/pull/2"));
        }
      }
    }
    QCOMPARE(linkedErrors, 1);
  }

  void testPreviousAttemptIdNested() {
    LegacyData data;

    QJsonObject sessA;
    sessA[QStringLiteral("id")] = QStringLiteral("A");

    QJsonObject sessB;
    sessB[QStringLiteral("id")] = QStringLiteral("B");
    QJsonObject reqB;
    reqB[QStringLiteral("previousAttemptId")] = QStringLiteral("A");
    sessB[QStringLiteral("request")] = reqB;

    QJsonObject sessC;
    sessC[QStringLiteral("id")] = QStringLiteral("C");
    QJsonObject reqC;
    reqC[QStringLiteral("previousAttemptId")] = QStringLiteral("A");
    sessC[QStringLiteral("request")] = reqC;

    QJsonObject sessD;
    sessD[QStringLiteral("id")] = QStringLiteral("D");
    QJsonObject reqD;
    reqD[QStringLiteral("previousAttemptId")] = QStringLiteral("B");
    sessD[QStringLiteral("request")] = reqD;

    // We add them in arbitrary order
    data.activeSessions.append(sessC);
    data.activeSessions.append(sessD);
    data.activeSessions.append(sessA);
    data.activeSessions.append(sessB);

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());
    QCOMPARE(res.jobs.size(), 1);
    QCOMPARE(res.jobs[0].attempts.size(), 4);

    // Test duplicate predecessor
    LegacyData badData;
    QJsonObject badA1;
    badA1[QStringLiteral("id")] = QStringLiteral("A");
    QJsonObject badA2;
    badA2[QStringLiteral("id")] = QStringLiteral("A");
    QJsonObject badB;
    badB[QStringLiteral("id")] = QStringLiteral("B");
    QJsonObject badReqB;
    badReqB[QStringLiteral("previousAttemptId")] = QStringLiteral("A");
    badB[QStringLiteral("request")] = badReqB;
    badData.activeSessions.append(badA1);
    badData.activeSessions.append(badA2);
    badData.activeSessions.append(badB);

    auto badRes = LegacyConverter::convertAll(badData, QDateTime::currentDateTimeUtc());
    QCOMPARE(badRes.jobs.size(), 3); // Must not merge!

    // Test cyclic
    LegacyData cycleData;
    QJsonObject cycA;
    cycA[QStringLiteral("id")] = QStringLiteral("A");
    QJsonObject cycReqA;
    cycReqA[QStringLiteral("previousAttemptId")] = QStringLiteral("B");
    cycA[QStringLiteral("request")] = cycReqA;
    QJsonObject cycB;
    cycB[QStringLiteral("id")] = QStringLiteral("B");
    QJsonObject cycReqB;
    cycReqB[QStringLiteral("previousAttemptId")] = QStringLiteral("A");
    cycB[QStringLiteral("request")] = cycReqB;
    cycleData.activeSessions.append(cycA);
    cycleData.activeSessions.append(cycB);
    auto cycRes = LegacyConverter::convertAll(cycleData, QDateTime::currentDateTimeUtc());
    QCOMPARE(cycRes.jobs.size(), 2); // Must not merge cyclic!
  }

  void testUnattachedErrors() {
    LegacyData data;

    QJsonObject opError;
    QJsonObject opReq;
    opReq[QStringLiteral("prompt")] = QStringLiteral("p"); // Add prompt to prove diagnostic overrides it
    opError[QStringLiteral("request")] = opReq;
    opError[QStringLiteral("message")] = QStringLiteral("op error");
    opError[QStringLiteral("operation")] = QStringLiteral("clone");
    opError[QStringLiteral("provider")] = QStringLiteral("github");

    QJsonObject workError;
    QJsonObject req;
    req[QStringLiteral("prompt")] = QStringLiteral("p");
    workError[QStringLiteral("request")] = req;
    workError[QStringLiteral("message")] = QStringLiteral("work error");

    QJsonObject sessError;
    QJsonObject sessReq;
    sessReq[QStringLiteral("prompt")] =
        QStringLiteral("sess_p"); // Needs prompt so it looks like work, otherwise unattached!
    sessError[QStringLiteral("request")] = sessReq;
    sessError[QStringLiteral("message")] = QStringLiteral("session error");
    sessError[QStringLiteral("sessionId")] = QStringLiteral("sess1"); // Belongs to an attempt

    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");
    sess1[QStringLiteral("prompt")] = QStringLiteral("s");
    data.activeSessions.append(sess1);

    data.errors.append(opError);
    data.errors.append(workError);
    data.errors.append(sessError);

    QDateTime ts = QDateTime::currentDateTimeUtc();
    auto res = LegacyConverter::convertAll(data, ts);

    QCOMPARE(res.jobs.size(), 2);             // The work error becomes a job (1), plus the session job (2)
    QCOMPARE(res.unattachedErrors.size(), 1); // The op error stays unattached

    bool foundAttError = false;
    for (const auto &j : res.jobs) {
      if (!j.attempts.isEmpty() && j.attempts.last().julesSessionId == QStringLiteral("sess1")) {
        QCOMPARE(j.attempts.last().launchErrors.size(), 1);
        foundAttError = true;
      }
    }
    QVERIFY(foundAttError);
  }

  void testMalformedStoreValidation() {
    JobStore store(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));

    QFile f(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
    f.open(QIODevice::WriteOnly);
    f.write("{\"schemaVersion\": 1, \"jobs\": [{\"id\": \"123\", \"priority\": \"not-a-number\"}]}");
    f.close();
    QVERIFY(!store.load()); // Should fail because priority isn't a number
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));

    f.open(QIODevice::WriteOnly);
    f.write("{\"schemaVersion\": 1, \"jobs\": [{\"id\": \"123\", \"acceptedAttemptId\": \"missing\"}]}");
    f.close();
    QVERIFY(!store.load()); // Should fail because acceptedAttemptId references nonexistent attempt
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/bad.json"));
  }
};

QTEST_MAIN(TestJob)

#include "test_job.moc"
