#include <QDebug>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTest>

class UniquenessPerformanceTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testListVsSet() {
    // Generate data mimicking selected rows, some duplicates
    QList<int> selectedRows;
    for (int i = 0; i < 50000; ++i) {
      selectedRows.append(i % 10000); // Many duplicates
    }

    // Benchmark QList::contains
    QElapsedTimer timer;
    timer.start();
    QList<int> rowsToDeleteList;
    for (int row : selectedRows) {
      if (!rowsToDeleteList.contains(row)) {
        rowsToDeleteList.append(row);
      }
    }
    std::sort(rowsToDeleteList.begin(), rowsToDeleteList.end(),
              std::greater<int>());
    qint64 listTime = timer.nsecsElapsed();

    // Benchmark QSet
    timer.start();
    QSet<int> uniqueRows;
    for (int row : selectedRows) {
      uniqueRows.insert(row);
    }
    QList<int> rowsToDeleteSet(uniqueRows.begin(), uniqueRows.end());
    std::sort(rowsToDeleteSet.begin(), rowsToDeleteSet.end(),
              std::greater<int>());
    qint64 setTime = timer.nsecsElapsed();

    qDebug() << "List hits (unique):" << rowsToDeleteList.size()
             << "Time (ns):" << listTime;
    qDebug() << "Set hits (unique):" << rowsToDeleteSet.size()
             << "Time (ns):" << setTime;
    qDebug() << "Improvement:" << (double)listTime / setTime << "x";

    QCOMPARE(rowsToDeleteList, rowsToDeleteSet);
  }
};

QTEST_MAIN(UniquenessPerformanceTest)
#include "bench_uniqueness.moc"
