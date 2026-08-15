#include "../src/configmigration.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QTemporaryFile>
#include <QtTest>

class TestConfigMigration : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testSupportedValuesSurvive();
  void testUnsupportedLegacyValuesMigrate();
  void testMergeRefreshAndQueueRemoved();
  void testMigrationIsIdempotent();
};

void TestConfigMigration::testSupportedValuesSurvive() {
  QTemporaryFile tempConfigFile;
  QVERIFY(tempConfigFile.open());
  QString tempConfigPath = tempConfigFile.fileName();
  tempConfigFile.close();

  KSharedConfigPtr config = KSharedConfig::openConfig(tempConfigPath, KConfig::SimpleConfig);
  KConfigGroup sessionConfig(config, QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry("FollowingAutoRefreshInterval", 1800); // 30 mins
  sessionConfig.sync();

  ConfigMigration::migrate(config);

  KConfigGroup updatedConfig(config, QStringLiteral("SessionWindow"));
  QCOMPARE(updatedConfig.readEntry("FollowingAutoRefreshInterval", 0), 1800);
}

void TestConfigMigration::testUnsupportedLegacyValuesMigrate() {
  QTemporaryFile tempConfigFile;
  QVERIFY(tempConfigFile.open());
  QString tempConfigPath = tempConfigFile.fileName();
  tempConfigFile.close();

  KSharedConfigPtr config = KSharedConfig::openConfig(tempConfigPath, KConfig::SimpleConfig);
  KConfigGroup sessionConfig(config, QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry("FollowingAutoRefreshInterval", -1);
  sessionConfig.sync();

  ConfigMigration::migrate(config);

  KConfigGroup updatedConfig(config, QStringLiteral("SessionWindow"));
  QCOMPARE(updatedConfig.readEntry("FollowingAutoRefreshInterval", 0), 900);

  // Test 0 as well
  sessionConfig.writeEntry("FollowingAutoRefreshInterval", 0);
  sessionConfig.sync();

  ConfigMigration::migrate(config);
  QCOMPARE(updatedConfig.readEntry("FollowingAutoRefreshInterval", 0), 900);
}

void TestConfigMigration::testMergeRefreshAndQueueRemoved() {
  QTemporaryFile tempConfigFile;
  QVERIFY(tempConfigFile.open());
  QString tempConfigPath = tempConfigFile.fileName();
  tempConfigFile.close();

  KSharedConfigPtr config = KSharedConfig::openConfig(tempConfigPath, KConfig::SimpleConfig);
  KConfigGroup sessionConfig(config, QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry("MergeRefreshAndQueue", true);
  sessionConfig.sync();

  QVERIFY(sessionConfig.hasKey("MergeRefreshAndQueue"));

  ConfigMigration::migrate(config);

  KConfigGroup updatedConfig(config, QStringLiteral("SessionWindow"));
  QVERIFY(!updatedConfig.hasKey("MergeRefreshAndQueue"));
}

void TestConfigMigration::testMigrationIsIdempotent() {
  QTemporaryFile tempConfigFile;
  QVERIFY(tempConfigFile.open());
  QString tempConfigPath = tempConfigFile.fileName();
  tempConfigFile.close();

  KSharedConfigPtr config = KSharedConfig::openConfig(tempConfigPath, KConfig::SimpleConfig);
  KConfigGroup sessionConfig(config, QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry("MergeRefreshAndQueue", true);
  sessionConfig.writeEntry("FollowingAutoRefreshInterval", -1);
  sessionConfig.sync();

  // Run migration twice
  ConfigMigration::migrate(config);
  ConfigMigration::migrate(config);

  KConfigGroup updatedConfig(config, QStringLiteral("SessionWindow"));
  QVERIFY(!updatedConfig.hasKey("MergeRefreshAndQueue"));
  QCOMPARE(updatedConfig.readEntry("FollowingAutoRefreshInterval", 0), 900);
}

QTEST_MAIN(TestConfigMigration)
#include "test_configmigration.moc"
