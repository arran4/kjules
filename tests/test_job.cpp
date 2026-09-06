#include <QStandardPaths>
#include <QTest>
#include <QObject>
#include <QUuid>
#include "../src/jobdata.h"
#include "../src/jobpolicy.h"
#include "../src/jobstore.h"
#include "../src/legacyconverter.h"

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

    void testJobPolicyNoAttempts() {
        JobData job;
        QCOMPARE(JobPolicy::consumesConcurrency(JobAttemptData()), true); // Default is empty which we don't have
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

        QCOMPARE(JobPolicy::consumesConcurrency(attempt), true);
        QCOMPARE(JobPolicy::hasViableActiveAttempt(job), true);
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

        JobStore store(QStringLiteral("test_jobs.json"));
        store.addJob(job);
        QVERIFY(store.save());

        JobStore loadedStore(QStringLiteral("test_jobs.json"));
        QVERIFY(loadedStore.load());
        QCOMPARE(loadedStore.jobs().size(), 1);
        QCOMPARE(loadedStore.jobs()[0].id, job.id);

        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kjules/test_jobs.json"));
    }

    void testLegacyQueueConversion() {
        QueueItem item;
        item.requestData[QStringLiteral("prompt")] = QStringLiteral("test prompt");
        QJsonObject ctx; ctx[QStringLiteral("source")] = QStringLiteral("test source");
        item.requestData[QStringLiteral("sourceContext")] = ctx;
        item.isBlocked = true;

        JobData job = LegacyConverter::fromQueueItem(item, false);
        QCOMPARE(job.prompt, QString(QStringLiteral("test prompt")));
        QCOMPARE(job.source, QString(QStringLiteral("test source")));
        QCOMPARE(job.attempts.size(), 0);
        QCOMPARE(job.lifecycleMetadata[QStringLiteral("isBlocked")].toBool(), true);
    }

    void testLegacySessionConversion() {
        QJsonObject session;
        session[QStringLiteral("id")] = QStringLiteral("session-123");
        session[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
        session[QStringLiteral("source")] = QStringLiteral("session-source");
        session[QStringLiteral("createTime")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        JobData job = LegacyConverter::fromSession(session, false);
        QCOMPARE(job.source, QString(QStringLiteral("session-source")));
        QCOMPARE(job.attempts.size(), 1);
        QCOMPARE(job.attempts[0].julesSessionId, QString(QStringLiteral("session-123")));
        QCOMPARE(job.attempts[0].julesState, QString(QStringLiteral("IN_PROGRESS")));
    }

    void testLegacyErrorConversion() {
        QJsonObject error;
        QJsonObject req; req[QStringLiteral("prompt")] = QStringLiteral("error prompt");
        error[QStringLiteral("request")] = req;
        error[QStringLiteral("message")] = QStringLiteral("error message");

        JobData job = LegacyConverter::fromError(error);
        QCOMPARE(job.prompt, QString(QStringLiteral("error prompt")));
        QCOMPARE(job.attempts.size(), 1);
        QCOMPARE(job.attempts[0].dispatchState, QString(QStringLiteral("FAILED")));
        QCOMPARE(job.attempts[0].launchErrors.size(), 1);
        QCOMPARE(job.attempts[0].launchErrors[0].toObject()[QStringLiteral("message")].toString(), QString(QStringLiteral("error message")));
    }

    void testDeterministicIdQueue() {
        QueueItem item;
        item.requestData[QStringLiteral("prompt")] = QStringLiteral("determ prompt");
        item.requestData[QStringLiteral("id")] = QStringLiteral("req-1");

        JobData job1 = LegacyConverter::fromQueueItem(item, false);
        JobData job2 = LegacyConverter::fromQueueItem(item, false);

        QCOMPARE(job1.id, job2.id); // Same input produces same Job ID

        // Change input slightly
        item.requestData[QStringLiteral("id")] = QStringLiteral("req-2");
        JobData job3 = LegacyConverter::fromQueueItem(item, false);
        QVERIFY(job1.id != job3.id); // Different input produces different ID
    }

    void testDeterministicIdSession() {
        QJsonObject session;
        session[QStringLiteral("id")] = QStringLiteral("sess-1");
        session[QStringLiteral("prompt")] = QStringLiteral("sess prompt");

        JobData job1 = LegacyConverter::fromSession(session, false);
        JobData job2 = LegacyConverter::fromSession(session, false);

        QCOMPARE(job1.id, job2.id);
        QVERIFY(job1.attempts.size() == 1);
        QVERIFY(job2.attempts.size() == 1);
        QCOMPARE(job1.attempts[0].id, job2.attempts[0].id); // Attempt IDs match
    }

    void testDeterministicIdError() {
        QJsonObject error;
        QJsonObject req; req[QStringLiteral("prompt")] = QStringLiteral("error prompt");
        error[QStringLiteral("request")] = req;
        error[QStringLiteral("message")] = QStringLiteral("error msg");

        JobData job1 = LegacyConverter::fromError(error);
        JobData job2 = LegacyConverter::fromError(error);

        QCOMPARE(job1.id, job2.id);
        QVERIFY(job1.attempts.size() == 1);
        QCOMPARE(job1.attempts[0].id, job2.attempts[0].id);
    }
};

QTEST_MAIN(TestJob)

#include "test_job.moc"
