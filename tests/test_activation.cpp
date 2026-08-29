
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QObject>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

class ActivationTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testArgumentParsing() {
    QCOMPARE(QStringLiteral(KJULES_APPLICATION_NAME), QStringLiteral("kjules"));
    QCOMPARE(QStringLiteral(KJULES_ORGANIZATION_DOMAIN), QStringLiteral("arran4.github.io"));
    QCOMPARE(QStringLiteral(KJULES_APPLICATION_ID), QStringLiteral("io.github.arran4.kjules"));

    QStringList args = {QStringLiteral(KJULES_APPLICATION_NAME), QStringLiteral("--new-session")};
    QCommandLineParser p;
    QCommandLineOption newSessionO(QStringList() << QStringLiteral("new-session"));
    p.addOption(newSessionO);
    p.parse(args);
    QVERIFY(p.isSet(newSessionO));
  }

  void testMigrationPaths() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());

    QString dataParent = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString oldDataPath = dataParent + QStringLiteral("/org.kde.kjules");
    QString newDataPath = dataParent + QStringLiteral("/") + QStringLiteral(KJULES_APPLICATION_NAME);

    QCOMPARE(oldDataPath, tempDir.path() + QStringLiteral("/org.kde.kjules"));
    QCOMPARE(newDataPath, tempDir.path() + QStringLiteral("/kjules"));

    QString configParent = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString oldConfigPath = configParent + QStringLiteral("/org.kde.kjulesrc");
    QString newConfigPath =
        configParent + QStringLiteral("/") + QStringLiteral(KJULES_APPLICATION_NAME) + QStringLiteral("rc");

    QCOMPARE(oldConfigPath, tempDir.path() + QStringLiteral("/org.kde.kjulesrc"));
    QCOMPARE(newConfigPath, tempDir.path() + QStringLiteral("/kjulesrc"));

    qunsetenv("XDG_DATA_HOME");
    qunsetenv("XDG_CONFIG_HOME");
  }

  void testMockIdentity() {
    QString origName = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("kjules-mock"));
    QString mockConfig =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kjules-mockrc");
    QVERIFY(mockConfig.endsWith(QStringLiteral("kjules-mockrc")));
    QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("kjules-mock"));
    QCoreApplication::setApplicationName(origName);
  }
};

QTEST_MAIN(ActivationTest)
#include "test_activation.moc"
