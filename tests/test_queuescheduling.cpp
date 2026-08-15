#include "../src/errorsmodel.h"
#include "../src/queuemodel.h"
#include "../src/queuescheduler.h"
#include "../src/utils.h"
#include <QDateTime>
#include <QJsonObject>
#include <QtTest>

class TestQueueScheduling : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testOrdinaryTimerIntervalRespected();
  void testImmediateDispatchEstablishesNextOrdinaryInterval();
  void testBackoffBlocksDirectProcessing();
  void testRetryBackoffOverridesLongerOrdinaryInterval();
  void testRetryableItemsRemainQueued();
  void testNonRetryableFailuresDoNotBlockUnrelatedWork();
  void testRetryExhaustionMovesToErrors();
  void testCountdownDisplayDoesNotMutateSchedulingState();
};

void TestQueueScheduling::testOrdinaryTimerIntervalRespected() {
  QueueScheduler::State state;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Initially uninitialized nextProcessAt -> isDue is true
  QVERIFY(state.isDue(now));

  // Advance for 15-minute interval
  state.advanceOnDispatch(now, 15);
  QCOMPARE(state.nextProcessAt, now.addSecs(15 * 60));

  // Before 15 minutes, not due
  QVERIFY(!state.isDue(now.addSecs(14 * 60)));

  // At 15 minutes, due
  QVERIFY(state.isDue(now.addSecs(15 * 60)));

  // After 15 minutes, due
  QVERIFY(state.isDue(now.addSecs(16 * 60)));
}

void TestQueueScheduling::testImmediateDispatchEstablishesNextOrdinaryInterval() {
  QueueScheduler::State state;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Immediate dispatch at 'now' with 10-minute interval
  state.advanceOnDispatch(now, 10);
  QCOMPARE(state.nextProcessAt, now.addSecs(10 * 60));
  QVERIFY(!state.backoffUntil.isValid());
  QVERIFY(state.backoffReason.isEmpty());
}

void TestQueueScheduling::testBackoffBlocksDirectProcessing() {
  QueueScheduler::State state;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // Ordinary schedule would be due
  state.nextProcessAt = now.addSecs(-60); // 1 minute overdue

  // Apply QBI backoff for 15 minutes
  state.applyBackoff(now, 15 * 60, QStringLiteral("Concurrent Limit Reached"));
  QVERIFY(state.isBackoffActive(now));
  QVERIFY(!state.isDue(now));
  QVERIFY(!state.isDue(now.addSecs(14 * 60)));

  // Once backoff expires, backoff is no longer active
  QVERIFY(!state.isBackoffActive(now.addSecs(15 * 60)));
  QVERIFY(state.isDue(now.addSecs(15 * 60)));
}

void TestQueueScheduling::testRetryBackoffOverridesLongerOrdinaryInterval() {
  QueueScheduler::State state;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);

  // TimerInterval = 60 minutes
  state.advanceOnDispatch(now, 60);
  QCOMPARE(state.nextProcessAt, now.addSecs(3600));

  // Error occurs 1 minute later, QBI = 15 minutes applied
  QDateTime errTime = now.addSecs(60);
  state.applyBackoff(errTime, 15 * 60, QStringLiteral("Error Processing Task"));

  // At 10 minutes from errTime (now + 11 min), backoff still active
  QVERIFY(state.isBackoffActive(errTime.addSecs(10 * 60)));
  QVERIFY(!state.isDue(errTime.addSecs(10 * 60)));

  // At 15 minutes from errTime (now + 16 min), backoff has expired
  // isDue must be true even though nextProcessAt is still at now + 60 min!
  QDateTime retryTime = errTime.addSecs(15 * 60);
  QVERIFY(!state.isBackoffActive(retryTime));
  QVERIFY(state.isDue(retryTime));
}

void TestQueueScheduling::testRetryableItemsRemainQueued() {
  QueueModel queueModel(nullptr, QStringLiteral("test_queue_retry.json"), true);
  queueModel.clear();

  QueueItem item;
  item.requestData[QStringLiteral("prompt")] = QStringLiteral("Test prompt");
  item.errorCount = 0;
  queueModel.enqueueItem(item);
  QCOMPARE(queueModel.rowCount(), 1);

  // Simulate retryable error attempt
  QDateTime now = QDateTime::currentDateTimeUtc();
  QueueItem current = queueModel.peek();
  current.errorCount++;
  current.lastError = QStringLiteral("Temporary network timeout");
  current.lastTry = now;
  queueModel.updateItem(0, current);

  // Item remains in queue with updated error count
  QCOMPARE(queueModel.rowCount(), 1);
  QCOMPARE(queueModel.peek().errorCount, 1);
  QCOMPARE(queueModel.peek().lastError, QStringLiteral("Temporary network timeout"));
}

void TestQueueScheduling::testNonRetryableFailuresDoNotBlockUnrelatedWork() {
  QueueModel queueModel(nullptr, QStringLiteral("test_queue_nonretry.json"), true);
  queueModel.clear();

  ErrorsModel errorsModel(nullptr);

  QueueItem item1;
  item1.requestData[QStringLiteral("prompt")] = QStringLiteral("Invalid task 1");
  queueModel.enqueueItem(item1);

  QueueItem item2;
  item2.requestData[QStringLiteral("prompt")] = QStringLiteral("Valid task 2");
  queueModel.enqueueItem(item2);

  QCOMPARE(queueModel.rowCount(), 2);

  // Non-retryable error on item 0 (e.g. 400 Validation Error)
  QueueItem failedItem = queueModel.peek();
  queueModel.removeItem(0);

  QJsonObject errObj;
  errObj[QStringLiteral("request")] = failedItem.requestData;
  errObj[QStringLiteral("message")] = QStringLiteral("Invalid parameters");
  errorsModel.addErrorObj(errObj);

  // Queue now has remaining item2 immediately ready
  QCOMPARE(queueModel.rowCount(), 1);
  QCOMPARE(queueModel.peek().requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Valid task 2"));
  QCOMPARE(errorsModel.rowCount(), 1);

  // Scheduling state for unrelated work has no active backoff
  QueueScheduler::State state;
  QDateTime now = QDateTime::currentDateTimeUtc();
  QVERIFY(!state.isBackoffActive(now));
}

void TestQueueScheduling::testRetryExhaustionMovesToErrors() {
  QueueModel queueModel(nullptr, QStringLiteral("test_queue_exhaustion.json"), true);
  queueModel.clear();

  ErrorsModel errorsModel(nullptr);

  QueueItem item;
  item.requestData[QStringLiteral("prompt")] = QStringLiteral("Failing task");
  item.errorCount = 3;
  queueModel.enqueueItem(item);

  // 4th failure triggers exhaustion
  QueueItem current = queueModel.peek();
  current.errorCount++;
  if (current.errorCount >= 4) {
    queueModel.removeItem(0);
    QJsonObject errObj;
    errObj[QStringLiteral("request")] = current.requestData;
    errObj[QStringLiteral("message")] = QStringLiteral("Server 500 error");
    errorsModel.addErrorObj(errObj);
  }

  QCOMPARE(queueModel.rowCount(), 0);
  QCOMPARE(errorsModel.rowCount(), 1);
}

void TestQueueScheduling::testCountdownDisplayDoesNotMutateSchedulingState() {
  QueueScheduler::State state;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  state.nextProcessAt = now.addSecs(180);
  state.backoffUntil = now.addSecs(300);
  state.backoffReason = QStringLiteral("Rate Limited");

  // Read-only inspection
  QDateTime savedNext = state.nextProcessAt;
  QDateTime savedBackoff = state.backoffUntil;
  QString savedReason = state.backoffReason;

  qint64 secondsLeft = now.secsTo(state.backoffUntil);
  QString durationStr = Utils::formatDuration(secondsLeft);
  QCOMPARE(durationStr, QStringLiteral("5m 0s"));

  // Verify state is completely unchanged
  QCOMPARE(state.nextProcessAt, savedNext);
  QCOMPARE(state.backoffUntil, savedBackoff);
  QCOMPARE(state.backoffReason, savedReason);
}

QTEST_MAIN(TestQueueScheduling)
#include "test_queuescheduling.moc"
