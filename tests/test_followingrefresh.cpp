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
  void testMergedPrDetection();
  void testSessionModelPrStatusWithNullMergedAt();
  void testInheritedPerSessionInterval();
  void testPerSessionDisabledState();
  void testPositivePerSessionOverride();
  void testDuplicateInFlightRefreshSuppression();
  void testFailureCooldownSuppressesSpam();
  void testFailureDoesNotModifyLastRefreshed();
  void testSuccessfulRefreshUpdatesAndPersistsLastRefreshed();
  void testTopLevelPullRequestParsed();
  void testOutputsPullRequestParsed();
};

void TestFollowingRefresh::testTopLevelPullRequestParsed() {
  QJsonObject session;
  session[QStringLiteral("id")] = QStringLiteral("sess-pr-1");
  session[QStringLiteral("title")] = QStringLiteral("Top Level PR");

  QJsonObject prObj;
  prObj[QStringLiteral("url")] = QStringLiteral("https://github.com/owner/repo/pull/123");
  session[QStringLiteral("pullRequest")] = prObj;

  // Directly parse session using SessionModel's parseSessionData logic (indirectly tested here)
  SessionModel model(QStringLiteral("test_following_pr_1.json"));
  model.clearSessions();
  model.addSession(session);

  QModelIndex idx = model.index(0, 0);
  QCOMPARE(model.data(idx, SessionModel::PrUrlRole).toString(),
           QStringLiteral("https://github.com/owner/repo/pull/123"));

  // PR number is displayed in ColPR, Qt::DisplayRole
  QModelIndex colPrIdx = model.index(0, SessionModel::ColPR);
  QCOMPARE(model.data(colPrIdx, Qt::DisplayRole).toString(), QStringLiteral("#123"));
}

void TestFollowingRefresh::testOutputsPullRequestParsed() {
  QJsonObject session;
  session[QStringLiteral("id")] = QStringLiteral("sess-pr-2");
  session[QStringLiteral("title")] = QStringLiteral("Outputs PR");

  QJsonObject prObj;
  prObj[QStringLiteral("url")] = QStringLiteral("https://github.com/owner/repo/pull/456");

  QJsonObject outputObj;
  outputObj[QStringLiteral("pullRequest")] = prObj;

  QJsonArray outputsArr;
  outputsArr.append(outputObj);
  session[QStringLiteral("outputs")] = outputsArr;

  SessionModel model(QStringLiteral("test_following_pr_2.json"));
  model.clearSessions();
  model.addSession(session);

  QModelIndex idx = model.index(0, 0);
  QCOMPARE(model.data(idx, SessionModel::PrUrlRole).toString(),
           QStringLiteral("https://github.com/owner/repo/pull/456"));

  QModelIndex colPrIdx = model.index(0, SessionModel::ColPR);
  QCOMPARE(model.data(colPrIdx, Qt::DisplayRole).toString(), QStringLiteral("#456"));
}

void TestFollowingRefresh::testSessionEligibility() {
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("RUNNING"), QString()));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QString()));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("open")));

  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("merged")));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"), QStringLiteral("CLOSED")));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("CANCELED"), QString()));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("ERROR"), QString()));
}

void TestFollowingRefresh::testMergedPrDetection() {
  // 1. state=open, merged_at=null (unmerged PR, must be eligible)
  QJsonObject openNull;
  openNull[QStringLiteral("state")] = QStringLiteral("open");
  openNull[QStringLiteral("merged_at")] = QJsonValue::Null;
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(openNull));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(openNull), QStringLiteral("open"));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                       FollowingRefreshEvaluator::extractPrStatus(openNull)));

  // 2. state=open, no merged_at field (must be eligible)
  QJsonObject openNoField;
  openNoField[QStringLiteral("state")] = QStringLiteral("open");
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(openNoField));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(openNoField), QStringLiteral("open"));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                       FollowingRefreshEvaluator::extractPrStatus(openNoField)));

  // 3. state=open, merged_at="" (empty string, must be eligible)
  QJsonObject openEmptyMergedAt;
  openEmptyMergedAt[QStringLiteral("state")] = QStringLiteral("open");
  openEmptyMergedAt[QStringLiteral("merged_at")] = QStringLiteral("");
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(openEmptyMergedAt));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(openEmptyMergedAt), QStringLiteral("open"));
  QVERIFY(FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                       FollowingRefreshEvaluator::extractPrStatus(openEmptyMergedAt)));

  // 4. Truly merged: state=closed, merged_at is a valid non-empty timestamp (must NOT be eligible)
  QJsonObject trulyMerged;
  trulyMerged[QStringLiteral("state")] = QStringLiteral("closed");
  trulyMerged[QStringLiteral("merged_at")] = QStringLiteral("2026-01-01T12:00:00Z");
  QVERIFY(FollowingRefreshEvaluator::isPrMerged(trulyMerged));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(trulyMerged), QStringLiteral("merged"));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                        FollowingRefreshEvaluator::extractPrStatus(trulyMerged)));

  // 5. Canonical merged state or open with merged_at timestamp
  QJsonObject openWithMergedAt;
  openWithMergedAt[QStringLiteral("state")] = QStringLiteral("open");
  openWithMergedAt[QStringLiteral("merged_at")] = QStringLiteral("2026-01-01T12:00:00Z");
  QVERIFY(FollowingRefreshEvaluator::isPrMerged(openWithMergedAt));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(openWithMergedAt), QStringLiteral("merged"));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                        FollowingRefreshEvaluator::extractPrStatus(openWithMergedAt)));

  QJsonObject canonicalMerged;
  canonicalMerged[QStringLiteral("state")] = QStringLiteral("merged");
  QVERIFY(FollowingRefreshEvaluator::isPrMerged(canonicalMerged));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(canonicalMerged), QStringLiteral("merged"));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                        FollowingRefreshEvaluator::extractPrStatus(canonicalMerged)));

  // 6. Closed-unmerged: state=closed, merged_at=null (must NOT be eligible because it is closed)
  QJsonObject closedUnmerged;
  closedUnmerged[QStringLiteral("state")] = QStringLiteral("closed");
  closedUnmerged[QStringLiteral("merged_at")] = QJsonValue::Null;
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(closedUnmerged));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(closedUnmerged), QStringLiteral("closed"));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                        FollowingRefreshEvaluator::extractPrStatus(closedUnmerged)));

  // 7. Closed-unmerged: state=closed, no merged_at field
  QJsonObject closedNoField;
  closedNoField[QStringLiteral("state")] = QStringLiteral("closed");
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(closedNoField));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(closedNoField), QStringLiteral("closed"));
  QVERIFY(!FollowingRefreshEvaluator::isSessionEligible(QStringLiteral("COMPLETED"),
                                                        FollowingRefreshEvaluator::extractPrStatus(closedNoField)));

  // 8. Explicit boolean merged flags
  QJsonObject mergedBoolTrue;
  mergedBoolTrue[QStringLiteral("merged")] = true;
  QVERIFY(FollowingRefreshEvaluator::isPrMerged(mergedBoolTrue));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(mergedBoolTrue), QStringLiteral("merged"));

  QJsonObject mergedBoolFalse;
  mergedBoolFalse[QStringLiteral("merged")] = false;
  mergedBoolFalse[QStringLiteral("state")] = QStringLiteral("open");
  QVERIFY(!FollowingRefreshEvaluator::isPrMerged(mergedBoolFalse));
  QCOMPARE(FollowingRefreshEvaluator::extractPrStatus(mergedBoolFalse), QStringLiteral("open"));

  // 9. Sibling attempts with different PR statuses
  QJsonObject sibOpenRaw;
  QJsonObject sibOpenPr;
  sibOpenPr[QStringLiteral("state")] = QStringLiteral("open");
  sibOpenPr[QStringLiteral("merged_at")] = QJsonValue::Null;
  sibOpenRaw[QStringLiteral("githubPrInfo")] = sibOpenPr;

  QJsonObject sibMergedRaw;
  QJsonObject sibMergedPr;
  sibMergedPr[QStringLiteral("state")] = QStringLiteral("closed");
  sibMergedPr[QStringLiteral("merged_at")] = QStringLiteral("2026-01-01T12:00:00Z");
  sibMergedRaw[QStringLiteral("githubPrInfo")] = sibMergedPr;

  QJsonObject sibClosedRaw;
  QJsonObject sibClosedPr;
  sibClosedPr[QStringLiteral("state")] = QStringLiteral("closed");
  sibClosedPr[QStringLiteral("merged_at")] = QJsonValue::Null;
  sibClosedRaw[QStringLiteral("githubPrInfo")] = sibClosedPr;

  // Active or completed attempt with open PR (merged_at null) remains eligible
  QVERIFY(FollowingRefreshEvaluator::isAttemptEligible(QStringLiteral("COMPLETED"), sibOpenRaw, QJsonObject()));
  QVERIFY(FollowingRefreshEvaluator::isAttemptEligible(QStringLiteral("IN_PROGRESS"), sibOpenRaw, QJsonObject()));

  // Sibling attempts that are closed-unmerged or merged are not eligible when COMPLETED
  QVERIFY(!FollowingRefreshEvaluator::isAttemptEligible(QStringLiteral("COMPLETED"), sibMergedRaw, QJsonObject()));
  QVERIFY(!FollowingRefreshEvaluator::isAttemptEligible(QStringLiteral("COMPLETED"), sibClosedRaw, QJsonObject()));

  // Sibling attempt via prMetadata fallback
  QJsonObject sibMetaOpen;
  sibMetaOpen[QStringLiteral("status")] = QStringLiteral("open");
  sibMetaOpen[QStringLiteral("merged_at")] = QJsonValue::Null;
  QVERIFY(FollowingRefreshEvaluator::isAttemptEligible(QStringLiteral("COMPLETED"), QJsonObject(), sibMetaOpen));
}

void TestFollowingRefresh::testSessionModelPrStatusWithNullMergedAt() {
  SessionModel model(QStringLiteral("test_following_pr_null.json"));
  model.clearSessions();

  QJsonObject session;
  session[QStringLiteral("id")] = QStringLiteral("sess-null-merged-at");
  session[QStringLiteral("title")] = QStringLiteral("PR with null merged_at");

  QJsonObject prInfo;
  prInfo[QStringLiteral("state")] = QStringLiteral("open");
  prInfo[QStringLiteral("merged_at")] = QJsonValue::Null;
  session[QStringLiteral("githubPrInfo")] = prInfo;

  model.addSession(session);
  QModelIndex idx = model.index(0, 0);
  QCOMPARE(model.data(idx, SessionModel::PrStatusRole).toString(), QStringLiteral("open"));
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
