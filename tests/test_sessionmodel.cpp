#include "../src/sessionmodel.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class TestSessionModel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testClientFieldsSurviveUpdateSession();
  void testClientFieldsSurviveAddSessions();
  void testClientFieldsSurviveUpdateSessionAt();
  void testUnrelatedUpdatesDoNotAlterLastRefreshed();
  void testRawAndParsedStateConsistency();
  void testLastRefreshedRole();
  void testPersistedLastRefreshedSurvivesModelRestart();
};

void TestSessionModel::testPersistedLastRefreshedSurvivesModelRestart() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString cacheFile = tempDir.filePath(QStringLiteral("test_sm_restart.json"));

  // Step 1: Populate model, set lastRefreshed and metadata, then save to disk
  {
    SessionModel model(cacheFile);
    model.clearSessions();

    QJsonObject initial;
    initial[QStringLiteral("id")] = QStringLiteral("sess-persist");
    initial[QStringLiteral("title")] = QStringLiteral("Persisted Session");
    initial[QStringLiteral("local_favourite")] = 5;
    initial[QStringLiteral("local_snooze_until")] = QStringLiteral("2026-08-25T15:30:00Z");
    initial[QStringLiteral("local_refreshInterval")] = 25;
    initial[QStringLiteral("lastRefreshed")] = QStringLiteral("2026-08-15T06:45:00Z");

    model.addSession(initial);
    model.saveSessions();
  }

  // Step 2: Create a completely fresh model instance with the same file and load
  {
    SessionModel reloadedModel(cacheFile);
    reloadedModel.loadSessions();

    QCOMPARE(reloadedModel.rowCount(), 1);
    QJsonObject result = reloadedModel.getSession(0);

    QCOMPARE(result.value(QStringLiteral("id")).toString(), QStringLiteral("sess-persist"));
    QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Persisted Session"));
    QCOMPARE(result.value(QStringLiteral("local_favourite")).toInt(), 5);
    QCOMPARE(result.value(QStringLiteral("local_snooze_until")).toString(), QStringLiteral("2026-08-25T15:30:00Z"));
    QCOMPARE(result.value(QStringLiteral("local_refreshInterval")).toInt(), 25);
    QCOMPARE(result.value(QStringLiteral("lastRefreshed")).toString(), QStringLiteral("2026-08-15T06:45:00Z"));

    QModelIndex idx = reloadedModel.index(0, 0);
    QVariant lrVal = reloadedModel.data(idx, SessionModel::LastRefreshedRole);
    QVERIFY(lrVal.isValid());
    QCOMPARE(lrVal.toDateTime(), QDateTime::fromString(QStringLiteral("2026-08-15T06:45:00Z"), Qt::ISODate));
    QCOMPARE(reloadedModel.data(idx, SessionModel::FavouriteRole).toInt(), 5);
  }
}

void TestSessionModel::testClientFieldsSurviveUpdateSession() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_update.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-1");
  initial[QStringLiteral("title")] = QStringLiteral("Initial Title");
  initial[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  initial[QStringLiteral("local_favourite")] = 3;
  initial[QStringLiteral("local_snooze_until")] = QStringLiteral("2026-08-16T12:00:00Z");
  initial[QStringLiteral("local_refreshInterval")] = 10;
  initial[QStringLiteral("lastRefreshed")] = QStringLiteral("2026-08-15T08:00:00Z");

  model.addSession(initial);
  QCOMPARE(model.rowCount(), 1);

  // Incoming API payload that does NOT have local_* fields
  QJsonObject incoming;
  incoming[QStringLiteral("id")] = QStringLiteral("sess-1");
  incoming[QStringLiteral("title")] = QStringLiteral("Updated Title");
  incoming[QStringLiteral("state")] = QStringLiteral("AWAITING_USER_FEEDBACK");

  model.updateSession(incoming, /*isSuccessfulRefresh=*/false);

  QJsonObject result = model.getSession(0);
  QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Updated Title"));
  QCOMPARE(result.value(QStringLiteral("state")).toString(), QStringLiteral("AWAITING_USER_FEEDBACK"));
  QCOMPARE(result.value(QStringLiteral("local_favourite")).toInt(), 3);
  QCOMPARE(result.value(QStringLiteral("local_snooze_until")).toString(), QStringLiteral("2026-08-16T12:00:00Z"));
  QCOMPARE(result.value(QStringLiteral("local_refreshInterval")).toInt(), 10);
  QCOMPARE(result.value(QStringLiteral("lastRefreshed")).toString(), QStringLiteral("2026-08-15T08:00:00Z"));

  // Check model data roles
  QModelIndex idx = model.index(0, 0);
  QCOMPARE(model.data(idx, SessionModel::FavouriteRole).toInt(), 3);
  QCOMPARE(model.data(idx, SessionModel::SnoozeUntilRole).toDateTime(),
           QDateTime::fromString(QStringLiteral("2026-08-16T12:00:00Z"), Qt::ISODate));
}

void TestSessionModel::testClientFieldsSurviveAddSessions() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_addsessions.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-2");
  initial[QStringLiteral("title")] = QStringLiteral("Batch Initial");
  initial[QStringLiteral("local_favourite")] = 1;
  initial[QStringLiteral("local_snooze_until")] = QStringLiteral("2026-08-17T00:00:00Z");
  initial[QStringLiteral("local_refreshInterval")] = 30;
  initial[QStringLiteral("lastRefreshed")] = QStringLiteral("2026-08-15T09:00:00Z");

  model.addSession(initial);

  // Incoming batch array containing an update for sess-2
  QJsonObject incoming;
  incoming[QStringLiteral("id")] = QStringLiteral("sess-2");
  incoming[QStringLiteral("title")] = QStringLiteral("Batch Updated");
  QJsonArray arr;
  arr.append(incoming);

  model.addSessions(arr);

  QJsonObject result = model.getSession(0);
  QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Batch Updated"));
  QCOMPARE(result.value(QStringLiteral("local_favourite")).toInt(), 1);
  QCOMPARE(result.value(QStringLiteral("local_snooze_until")).toString(), QStringLiteral("2026-08-17T00:00:00Z"));
  QCOMPARE(result.value(QStringLiteral("local_refreshInterval")).toInt(), 30);
  QCOMPARE(result.value(QStringLiteral("lastRefreshed")).toString(), QStringLiteral("2026-08-15T09:00:00Z"));
}

void TestSessionModel::testClientFieldsSurviveUpdateSessionAt() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_updateat.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-3");
  initial[QStringLiteral("title")] = QStringLiteral("Initial");
  initial[QStringLiteral("local_favourite")] = 2;
  initial[QStringLiteral("local_snooze_until")] = QStringLiteral("2026-08-18T00:00:00Z");
  initial[QStringLiteral("local_refreshInterval")] = 5;
  initial[QStringLiteral("lastRefreshed")] = QStringLiteral("2026-08-15T11:00:00Z");

  model.addSession(initial);

  QJsonObject replacement;
  replacement[QStringLiteral("id")] = QStringLiteral("sess-3");
  replacement[QStringLiteral("title")] = QStringLiteral("Replacement Title");

  model.updateSessionAt(0, replacement, /*isSuccessfulRefresh=*/false);

  QJsonObject result = model.getSession(0);
  QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Replacement Title"));
  QCOMPARE(result.value(QStringLiteral("local_favourite")).toInt(), 2);
  QCOMPARE(result.value(QStringLiteral("local_snooze_until")).toString(), QStringLiteral("2026-08-18T00:00:00Z"));
  QCOMPARE(result.value(QStringLiteral("local_refreshInterval")).toInt(), 5);
  QCOMPARE(result.value(QStringLiteral("lastRefreshed")).toString(), QStringLiteral("2026-08-15T11:00:00Z"));
}

void TestSessionModel::testUnrelatedUpdatesDoNotAlterLastRefreshed() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_unrelated.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-4");
  initial[QStringLiteral("title")] = QStringLiteral("Original");
  QString lr = QStringLiteral("2026-08-15T05:00:00Z");
  initial[QStringLiteral("lastRefreshed")] = lr;

  model.addSession(initial);

  // Set favourite rank
  model.setFavouriteRank(QStringLiteral("sess-4"), 5);
  QCOMPARE(model.getSession(0).value(QStringLiteral("lastRefreshed")).toString(), lr);

  // Set snooze
  model.setSnoozeUntil(QStringLiteral("sess-4"),
                       QDateTime::fromString(QStringLiteral("2026-08-20T00:00:00Z"), Qt::ISODate));
  QCOMPARE(model.getSession(0).value(QStringLiteral("lastRefreshed")).toString(), lr);

  // Set refresh interval
  model.setRefreshInterval(QStringLiteral("sess-4"), 60);
  QCOMPARE(model.getSession(0).value(QStringLiteral("lastRefreshed")).toString(), lr);
}

void TestSessionModel::testRawAndParsedStateConsistency() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_consistency.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-5");
  initial[QStringLiteral("title")] = QStringLiteral("Consistency Test");
  initial[QStringLiteral("local_refreshInterval")] = 20;

  model.addSession(initial);

  // Update refresh interval to -1 (reset)
  model.setRefreshInterval(QStringLiteral("sess-5"), -1);

  QJsonObject raw = model.getSession(0);
  QVERIFY(!raw.contains(QStringLiteral("local_refreshInterval")));

  // Update refresh interval to 45
  model.setRefreshInterval(QStringLiteral("sess-5"), 45);
  raw = model.getSession(0);
  QCOMPARE(raw.value(QStringLiteral("local_refreshInterval")).toInt(), 45);
}

void TestSessionModel::testLastRefreshedRole() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SessionModel model(tempDir.filePath(QStringLiteral("test_sm_lastrefreshed.json")));
  model.clearSessions();

  QJsonObject initial;
  initial[QStringLiteral("id")] = QStringLiteral("sess-6");
  initial[QStringLiteral("title")] = QStringLiteral("Role Test");
  QDateTime lrTime = QDateTime::fromString(QStringLiteral("2026-08-15T07:30:00Z"), Qt::ISODate);
  initial[QStringLiteral("lastRefreshed")] = lrTime.toString(Qt::ISODate);

  model.addSession(initial);

  QModelIndex idx = model.index(0, 0);
  QVariant roleData = model.data(idx, SessionModel::LastRefreshedRole);
  QVERIFY(roleData.isValid());
  QCOMPARE(roleData.toDateTime(), lrTime);
}

QTEST_MAIN(TestSessionModel)
#include "test_sessionmodel.moc"
