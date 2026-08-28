#include <QObject>
#include <QTest>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>

class ActivationTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testArgumentParsing() {
    QStringList args = {QStringLiteral("kjules"), QStringLiteral("--new-session")};
    QCommandLineParser p;
    QCommandLineOption newSessionO(QStringList() << QStringLiteral("new-session"));
    p.addOption(newSessionO);
    p.parse(args);
    QVERIFY(p.isSet(newSessionO));
  }
};

QTEST_MAIN(ActivationTest)
#include "test_activation.moc"
