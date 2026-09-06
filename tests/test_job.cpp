#include "../src/jobdata.h"
#include "../src/jobpolicy.h"
#include "../src/jobstore.h"
#include "../src/legacyconverter.h"
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

  void testIdempotentReordering() {
    QueueItem item1;
    item1.requestData[QStringLiteral("id")] = QStringLiteral("req-1");
    QueueItem item2;
    item2.requestData[QStringLiteral("id")] = QStringLiteral("req-2");

    LegacyData data1;
    data1.queueItems = {item1, item2, item1};
    LegacyData data2;
    data2.queueItems = {item1, item1, item2}; // Reordered

    QDateTime ts = QDateTime::currentDateTimeUtc();
    auto res1 = LegacyConverter::convertAll(data1, ts);
    auto res2 = LegacyConverter::convertAll(data2, ts);

    QCOMPARE(res1.jobs.size(), 3);
    QCOMPARE(res2.jobs.size(), 3);

    // Check IDs
    QSet<QString> ids1;
    for (auto &j : res1.jobs)
      ids1.insert(j.id);
    QSet<QString> ids2;
    for (auto &j : res2.jobs)
      ids2.insert(j.id);

    QCOMPARE(ids1, ids2);
  }

  void testPreviousAttemptIdNested() {
    LegacyData data;

    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("sess1");

    QJsonObject sess2;
    sess2[QStringLiteral("id")] = QStringLiteral("sess2");
    QJsonObject req2;
    req2[QStringLiteral("previousAttemptId")] = QStringLiteral("sess1");
    sess2[QStringLiteral("request")] = req2;

    data.activeSessions.append(sess1);
    data.activeSessions.append(sess2);

    auto res = LegacyConverter::convertAll(data, QDateTime::currentDateTimeUtc());
    QCOMPARE(res.jobs.size(), 1);
    QCOMPARE(res.jobs[0].attempts.size(), 2);
  }

  void testUnattachedErrors() {
    LegacyData data;

    QJsonObject opError;
    opError[QStringLiteral("request")] = QJsonObject();
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
  }
};

QTEST_MAIN(TestJob)

#include "test_job.moc"
