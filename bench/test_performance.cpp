#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTest>

class PerformanceTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testListVsSet() {
    // Setup data
    QStringList sources;
    for (int i = 0; i < 5000; ++i) {
      sources.append(QStringLiteral("Source_") + QString::number(i));
    }

    QStringList testNames;
    for (int i = 0; i < 10000; ++i) {
      testNames.append(QStringLiteral("Source_") + QString::number(i));
    }

    // Benchmark QStringList
    QElapsedTimer timer;
    timer.start();
    int listHits = 0;
    for (const QString &name : testNames) {
      if (sources.contains(name)) {
        listHits++;
      }
    }
    qint64 listTime = timer.nsecsElapsed();

    // Benchmark QSet
    timer.start();
    QSet<QString> sourcesSet(sources.begin(), sources.end());
    int setHits = 0;
    for (const QString &name : testNames) {
      if (sourcesSet.contains(name)) {
        setHits++;
      }
    }
    qint64 setTime = timer.nsecsElapsed();

    qDebug() << "List hits:" << listHits << "Time (ns):" << listTime;
    qDebug() << "Set hits:" << setHits << "Time (ns):" << setTime;
    qDebug() << "Improvement:" << (double)listTime / setTime << "x";

    QCOMPARE(listHits, setHits);
  }
};

QTEST_MAIN(PerformanceTest)
#include "test_performance.moc"
