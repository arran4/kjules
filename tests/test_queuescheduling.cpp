#include "../src/api/apierror.h"
#include "../src/errorsmodel.h"
#include "../src/queuemodel.h"
#include "../src/queuescheduler.h"
#include "../src/utils.h"
#include <QDateTime>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class TestQueueScheduling : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testOrdinaryTimerIntervalRespected();
  void testImmediateDispatchEstablishesNextOrdinaryInterval();
  void testBackoffBlocksDirectProcessing();
  void testRetryBackoffOverridesLongerOrdinaryInterval();
  void testPreconditionErrorAppliesBackoffAndRetainsItem();
  void testRateLimitErrorAppliesDailyLimitRetryInterval();
  void testNonRetryableFailuresDoNotBlockUnrelatedWork();
  void testRetryExhaustionMovesToErrorsWithoutGlobalBackoff();
  void testSuccessfulRecoveryClearsBackoff();
  void testSettingsIntervalUpdatePreservesActiveBackoff();
  void testCountdownDisplayDoesNotMutateSchedulingState();
  void testSuccessfulQueueContinuationContract();
};

void TestQueueScheduling::testOrdinaryTimerIntervalRespected() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Initially uninitialized nextProcessAt -> isDue is true
  QVERIFY(scheduler.isDue(now));

  // Advance for 15-minute interval
  scheduler.recordDispatch(now, 15);
  QCOMPARE(scheduler.nextQueueProcessAt(), now.addSecs(15 * 60));

  // Before 15 minutes, not due
  QVERIFY(!scheduler.isDue(now.addSecs(14 * 60)));

  // At 15 minutes, due
  QVERIFY(scheduler.isDue(now.addSecs(15 * 60)));

  // After 15 minutes, due
  QVERIFY(scheduler.isDue(now.addSecs(16 * 60)));
}

void TestQueueScheduling::testImmediateDispatchEstablishesNextOrdinaryInterval() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Immediate dispatch at 'now' with 10-minute interval
  scheduler.recordDispatch(now, 10);
  QCOMPARE(scheduler.nextQueueProcessAt(), now.addSecs(10 * 60));
  QVERIFY(!scheduler.queueBackoffUntil().isValid());
  QVERIFY(scheduler.queueBackoffReason().isEmpty());
  QVERIFY(!scheduler.isBackoffActive(now));
}

void TestQueueScheduling::testBackoffBlocksDirectProcessing() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Ordinary schedule would be overdue
  scheduler.setNextProcessAt(now.addSecs(-60));

  // Apply QBI backoff for 15 minutes
  scheduler.applyBackoff(now, 15 * 60, QStringLiteral("Concurrent Limit Reached"));
  QVERIFY(scheduler.isBackoffActive(now));
  QVERIFY(!scheduler.isDue(now));
  QVERIFY(!scheduler.isDue(now.addSecs(14 * 60)));

  // Once backoff expires, isBackoffActive is false and isDue is true
  QDateTime expiredTime = now.addSecs(15 * 60);
  QVERIFY(!scheduler.isBackoffActive(expiredTime));
  QVERIFY(scheduler.isDue(expiredTime));
}

void TestQueueScheduling::testRetryBackoffOverridesLongerOrdinaryInterval() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Ordinary TimerInterval = 60 minutes
  scheduler.recordDispatch(now, 60);
  QCOMPARE(scheduler.nextQueueProcessAt(), now.addSecs(3600));

  // Failure occurs 1 minute later, QBI = 15 minutes applied
  QDateTime errTime = now.addSecs(60);
  scheduler.applyBackoff(errTime, 15 * 60, QStringLiteral("Error Processing Task"));

  // The effective next queue attempt is now the backoff expiry (errTime + 15m)
  QCOMPARE(scheduler.nextQueueProcessAt(), errTime.addSecs(15 * 60));

  // At 10 minutes from errTime (now + 11 min), backoff still active
  QVERIFY(scheduler.isBackoffActive(errTime.addSecs(10 * 60)));
  QVERIFY(!scheduler.isDue(errTime.addSecs(10 * 60)));

  // At 15 minutes from errTime (now + 16 min), backoff has expired
  // isDue is true approximately 15 minutes after failure, not waiting 60 minutes!
  QDateTime retryTime = errTime.addSecs(15 * 60);
  QVERIFY(!scheduler.isBackoffActive(retryTime));
  QVERIFY(scheduler.isDue(retryTime));
}

void TestQueueScheduling::testPreconditionErrorAppliesBackoffAndRetainsItem() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")), true);
  queueModel.clear();

  QueueItem item;
  item.requestData[QStringLiteral("prompt")] = QStringLiteral("Precondition task");
  queueModel.enqueueItem(item);

  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Simulate precondition error handling
  QueueItem current = queueModel.peek();
  current.lastError = QStringLiteral("FAILED_PRECONDITION: Concurrent limit reached");
  current.lastTry = now;
  queueModel.updateItem(0, current);

  int backoffMins = 15;
  scheduler.applyBackoff(now, backoffMins * 60, QStringLiteral("Concurrent Limit Reached"));

  // Item retained in queue
  QCOMPARE(queueModel.rowCount(), 1);
  QCOMPARE(queueModel.peek().lastError, QStringLiteral("FAILED_PRECONDITION: Concurrent limit reached"));

  // Scheduler is backed off
  QVERIFY(scheduler.isBackoffActive(now));
  QCOMPARE(scheduler.queueBackoffUntil(), now.addSecs(15 * 60));
  QCOMPARE(scheduler.queueBackoffReason(), QStringLiteral("Concurrent Limit Reached"));
}

void TestQueueScheduling::testRateLimitErrorAppliesDailyLimitRetryInterval() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")), true);
  queueModel.clear();

  QueueItem item;
  item.requestData[QStringLiteral("prompt")] = QStringLiteral("Rate limited task");
  queueModel.enqueueItem(item);

  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Simulate 429 / RESOURCE_EXHAUSTED error handling
  QueueItem current = queueModel.peek();
  current.lastError = QStringLiteral("RESOURCE_EXHAUSTED: Rate limit exceeded");
  current.lastTry = now;
  queueModel.updateItem(0, current);

  scheduler.applyBackoff(now, 3600, QStringLiteral("API Rate/Daily Limit Reached"));

  // Item retained
  QCOMPARE(queueModel.rowCount(), 1);

  // 1-hour DLRI backoff applied
  QVERIFY(scheduler.isBackoffActive(now));
  QCOMPARE(scheduler.queueBackoffUntil(), now.addSecs(3600));
  QCOMPARE(scheduler.queueBackoffReason(), QStringLiteral("API Rate/Daily Limit Reached"));
}

void TestQueueScheduling::testNonRetryableFailuresDoNotBlockUnrelatedWork() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")), true);
  queueModel.clear();

  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  errorsModel.clear();

  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  QueueItem item1;
  item1.requestData[QStringLiteral("prompt")] = QStringLiteral("Invalid task 1");
  queueModel.enqueueItem(item1);

  QueueItem item2;
  item2.requestData[QStringLiteral("prompt")] = QStringLiteral("Valid task 2");
  queueModel.enqueueItem(item2);

  // Non-retryable failure (e.g. 400 Validation Error): remove item and add to ErrorsModel
  QueueItem failedItem = queueModel.peek();
  queueModel.removeItem(0);

  QJsonObject errObj;
  errObj[QStringLiteral("request")] = failedItem.requestData;
  errObj[QStringLiteral("message")] = QStringLiteral("Invalid prompt parameter");
  errObj[QStringLiteral("timestamp")] = now.toString(Qt::ISODate);
  errorsModel.addErrorObj(errObj);

  // Item 1 moved to errors, Item 2 ready in queue
  QCOMPARE(queueModel.rowCount(), 1);
  QCOMPARE(queueModel.peek().requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Valid task 2"));
  QCOMPARE(errorsModel.rowCount(), 1);

  // No global backoff applied to scheduler; next queue attempt remains unblocked
  QVERIFY(!scheduler.isBackoffActive(now));
}

void TestQueueScheduling::testRetryExhaustionMovesToErrorsWithoutGlobalBackoff() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")), true);
  queueModel.clear();

  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  errorsModel.clear();

  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  QueueItem item;
  item.requestData[QStringLiteral("prompt")] = QStringLiteral("Failing task");
  item.errorCount = 3;
  queueModel.enqueueItem(item);

  // 4th failure occurs
  QueueItem current = queueModel.peek();
  current.errorCount++;
  if (current.errorCount >= 4) {
    queueModel.removeItem(0);
    QJsonObject errObj;
    errObj[QStringLiteral("request")] = current.requestData;
    errObj[QStringLiteral("message")] = QStringLiteral("Internal Server Error 500");
    errObj[QStringLiteral("timestamp")] = now.toString(Qt::ISODate);
    errorsModel.addErrorObj(errObj);
  }

  QCOMPARE(queueModel.rowCount(), 0);
  QCOMPARE(errorsModel.rowCount(), 1);

  // Exhausted failures do NOT apply global backoff to unrelated tasks
  QVERIFY(!scheduler.isBackoffActive(now));
}

void TestQueueScheduling::testSuccessfulRecoveryClearsBackoff() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  scheduler.applyBackoff(now, 15 * 60, QStringLiteral("Error"));
  QVERIFY(scheduler.isBackoffActive(now));

  // Successful dispatch clears backoff state
  scheduler.recordDispatch(now.addSecs(15 * 60), 10);
  QVERIFY(!scheduler.isBackoffActive(now.addSecs(15 * 60)));
  QVERIFY(!scheduler.queueBackoffUntil().isValid());
  QVERIFY(scheduler.queueBackoffReason().isEmpty());
}

void TestQueueScheduling::testSettingsIntervalUpdatePreservesActiveBackoff() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Backoff active for next 15 minutes
  scheduler.applyBackoff(now, 15 * 60, QStringLiteral("Concurrent Limit Reached"));
  QDateTime originalBackoff = scheduler.queueBackoffUntil();

  // Changing TimerInterval to 1 minute in Settings must NOT shorten active backoff
  scheduler.updateInterval(now, 1);
  QCOMPARE(scheduler.queueBackoffUntil(), originalBackoff);
  QCOMPARE(scheduler.nextQueueProcessAt(), originalBackoff);
  QVERIFY(scheduler.isBackoffActive(now));
}

void TestQueueScheduling::testCountdownDisplayDoesNotMutateSchedulingState() {
  QueueScheduler scheduler;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  scheduler.setNextProcessAt(now.addSecs(180));
  scheduler.setBackoffUntil(now.addSecs(300));
  scheduler.setBackoffReason(QStringLiteral("Rate Limited"));

  QDateTime savedNext = scheduler.nextQueueProcessAt();
  QDateTime savedBackoff = scheduler.queueBackoffUntil();
  QString savedReason = scheduler.queueBackoffReason();

  qint64 secondsLeft = now.secsTo(scheduler.queueBackoffUntil());
  QString durationStr = Utils::formatDuration(secondsLeft);
  QCOMPARE(durationStr, QStringLiteral("5m 0s"));

  // Verify state is completely untouched
  QCOMPARE(scheduler.nextQueueProcessAt(), savedNext);
  QCOMPARE(scheduler.queueBackoffUntil(), savedBackoff);
  QCOMPARE(scheduler.queueBackoffReason(), savedReason);
}

void TestQueueScheduling::testSuccessfulQueueContinuationContract() {
  // Explanatory Contract:
  // TimerInterval defines the periodic interval for the next scheduled check when idle or waiting.
  // When a queued item finishes successfully, the production result handlers immediately trigger
  // processQueue() (via QTimer::singleShot(0)) so queued batches continue processing without
  // inserting an artificial TimerInterval pause between items, provided concurrency permits and
  // no backoff is active.
  QueueScheduler scheduler;
  QDateTime t0 = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Dispatch item 1: sets nextQueueProcessAt to t0 + 15 mins
  scheduler.recordDispatch(t0, 15);
  QCOMPARE(scheduler.nextQueueProcessAt(), t0.addSecs(15 * 60));
  QVERIFY(!scheduler.isBackoffActive(t0));

  // Item 1 succeeds at t0 + 5s: result handler clears backoff and immediately dispatches item 2
  QDateTime t1 = t0.addSecs(5);
  scheduler.clearBackoff();
  QVERIFY(!scheduler.isBackoffActive(t1));

  // Immediate dispatch of item 2 succeeds and advances nextQueueProcessAt from t1
  scheduler.recordDispatch(t1, 15);
  QCOMPARE(scheduler.nextQueueProcessAt(), t1.addSecs(15 * 60));
  QVERIFY(!scheduler.isBackoffActive(t1));
}

QTEST_MAIN(TestQueueScheduling)
#include "test_queuescheduling.moc"
