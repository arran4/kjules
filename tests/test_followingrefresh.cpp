#include "../src/followingrefreshevaluator.h"
#include "../src/sessionmodel.h"
#include <QDateTime>
#include <QJsonObject>
#include <QtTest>

class TestFollowingRefresh : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testGlobalIntervalEvaluated();
  void testSessionEligibility();
  void testInheritedPerSessionInterval();
  void testPerSessionDisabledState();
  void testPositivePerSessionOverride();
  void testDuplicateInFlightRefreshSuppression();
  void testFailureCooldownSuppressesSpam();
  void testFailureDoesNotModifyLastRefreshed();
  void testSuccessfulRefreshUpdatesAndPersistsLastRefreshed();
};

void TestFollowingRefresh::testSessionEligibility() {
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("RUNNING"), QString()));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QString()));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("open")));

  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("merged")));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("CLOSED")));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("CANCELED"), QString()));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("ERROR"), QString()));
}

void TestFollowingRefresh::testGlobalIntervalEvaluated() {
  int globalIntervalSecs = 900; // 15 mins
  int effective = FollowingRefreshEvaluator::effectiveIntervalSeconds(globalIntervalSecs, std::nullopt);
  QCOMPARE(effective, 900);

  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  QDateTime refreshedAt = now.addSecs(-899); // 1 sec before due

  QVERIFY(!FollowingRefreshEvaluator::shouldRefresh(now, refreshedAt, effective, false, QDateTime()));

  // Exactly at 900s
  QVERIFY(FollowingRefreshEvaluator::shouldRefresh(now, now.addSecs(-900), effective, false, QDateTime()));
}

void TestFollowingRefresh::testInheritedPerSessionInterval() {
  int globalIntervalSecs = 900;
  // -1 means inherit global
  int effective = FollowingRefreshEvaluator::effectiveIntervalSeconds(globalIntervalSecs, -1);
  QCOMPARE(effective, 900);

  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  QVERIFY(FollowingRefreshEvaluator::shouldRefresh(now, now.addSecs(-900), effective, false, QDateTime()));
}

void TestFollowingRefresh::testPerSessionDisabledState() {
  int globalIntervalSecs = 900;
  // 0 means disabled
  int effective = FollowingRefreshEvaluator::effectiveIntervalSeconds(globalIntervalSecs, 0);
  QCOMPARE(effective, 0);

  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  // Even if lastRefreshed is 10 days ago, should never refresh when disabled
  QVERIFY(!FollowingRefreshEvaluator::shouldRefresh(now, now.addSecs(-864000), effective, false, QDateTime()));
}

void TestFollowingRefresh::testPositivePerSessionOverride() {
  int globalIntervalSecs = 900; // 15 mins
  // Local override = 5 minutes
  int effective = FollowingRefreshEvaluator::effectiveIntervalSeconds(globalIntervalSecs, 5);
  QCOMPARE(effective, 300);

  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  // At 4 mins ago -> not due
  QVERIFY(!FollowingRefreshEvaluator::shouldRefresh(now, now.addSecs(-240), effective, false, QDateTime()));

  // At 5 mins ago -> due!
  QVERIFY(FollowingRefreshEvaluator::shouldRefresh(now, now.addSecs(-300), effective, false, QDateTime()));
}

void TestFollowingRefresh::testDuplicateInFlightRefreshSuppression() {
  int effective = 900;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  QDateTime staleTime = now.addSecs(-3600);

  // If already in flight, should NOT trigger another reload
  QVERIFY(!FollowingRefreshEvaluator::shouldRefresh(now, staleTime, effective, /*isInFlight=*/true, QDateTime()));

  // When not in flight, should refresh
  QVERIFY(FollowingRefreshEvaluator::shouldRefresh(now, staleTime, effective, /*isInFlight=*/false, QDateTime()));
}

void TestFollowingRefresh::testFailureCooldownSuppressesSpam() {
  int effective = 900;
  QDateTime now = QDateTime::fromString(QStringLiteral("2026-08-15T12:00:00Z"), Qt::ISODate);
  QDateTime staleTime = now.addSecs(-3600);
  QDateTime recentFailure = now.addSecs(-60); // Failed 1 minute ago (cooldown is 300s)

  // Recent failure suppresses retry on the minute timer
  QVERIFY(!FollowingRefreshEvaluator::shouldRefresh(now, staleTime, effective, false, recentFailure, 300));

  // Once cooldown passes, it becomes eligible again
  QDateTime oldFailure = now.addSecs(-301);
  QVERIFY(FollowingRefreshEvaluator::shouldRefresh(now, staleTime, effective, false, oldFailure, 300));
}

void TestFollowingRefresh::testFailureDoesNotModifyLastRefreshed() {
  SessionModel model(QStringLiteral("test_following_fail.json"));
  model.clearSessions();

  QJsonObject session;
  session[QStringLiteral("id")] = QStringLiteral("sess-1");
  session[QStringLiteral("title")] = QStringLiteral("Session 1");
  QString initialLastRefreshed = QStringLiteral("2026-08-15T10:00:00Z");
  session[QStringLiteral("lastRefreshed")] = initialLastRefreshed;

  model.addSession(session);
  QCOMPARE(model.rowCount(), 1);

  // Generic update (e.g. state change or unrelated event) must NOT update lastRefreshed
  QJsonObject updateObj;
  updateObj[QStringLiteral("id")] = QStringLiteral("sess-1");
  updateObj[QStringLiteral("state")] = QStringLiteral("RUNNING");
  model.updateSession(updateObj, /*isSuccessfulRefresh=*/false);

  QJsonObject result = model.getSession(0);
  QCOMPARE(result.value(QStringLiteral("lastRefreshed")).toString(), initialLastRefreshed);
}

void TestFollowingRefresh::testSuccessfulRefreshUpdatesAndPersistsLastRefreshed() {
  SessionModel model(QStringLiteral("test_following_success.json"));
  model.clearSessions();

  QJsonObject session;
  session[QStringLiteral("id")] = QStringLiteral("sess-2");
  session[QStringLiteral("title")] = QStringLiteral("Session 2");
  QString initialLastRefreshed = QStringLiteral("2026-08-15T10:00:00Z");
  session[QStringLiteral("lastRefreshed")] = initialLastRefreshed;

  model.addSession(session);

  // Successful API reload must update lastRefreshed
  QJsonObject reloadObj;
  reloadObj[QStringLiteral("id")] = QStringLiteral("sess-2");
  reloadObj[QStringLiteral("title")] = QStringLiteral("Session 2 Reloaded");
  model.updateSession(reloadObj, /*isSuccessfulRefresh=*/true);

  QJsonObject result = model.getSession(0);
  QString newLastRefreshed = result.value(QStringLiteral("lastRefreshed")).toString();
  QVERIFY(!newLastRefreshed.isEmpty());
  QVERIFY(newLastRefreshed != initialLastRefreshed);
  QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Session 2 Reloaded"));
}

QTEST_MAIN(TestFollowingRefresh)
#include "test_followingrefresh.moc"
