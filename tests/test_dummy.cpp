#include <QTest>
#include <QObject>

class DummyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDummy()
    {
        QVERIFY(true);
    }
};

QTEST_MAIN(DummyTest)
#include "test_dummy.moc"
