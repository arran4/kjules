
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QObject>
#include <QStringList>
#include <QTest>

class ActivationTest : public QObject {
  Q_OBJECT

  // cppcheck-suppress unknownMacro
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
};

QTEST_MAIN(ActivationTest)
#include "test_activation.moc"
