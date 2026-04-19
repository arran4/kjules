#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <vector>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  // Setup
  QStringList list;
  for (int i = 0; i < 10000; ++i) {
    list.append(QStringLiteral("Source_%1").arg(i));
  }

  QStringList testNames;
  for (int i = 0; i < 20000; ++i) {
    testNames.append(QStringLiteral("Source_%1").arg(i));
  }

  // Benchmark QStringList
  QElapsedTimer timer;
  timer.start();
  int listHits = std::count_if(
      testNames.begin(), testNames.end(),
      [&list](const QString &name) { return list.contains(name); });
  qint64 listTime = timer.nsecsElapsed();

  // Benchmark QSet
  timer.start();
  QSet<QString> set(list.begin(), list.end());
  int setHits =
      std::count_if(testNames.begin(), testNames.end(),
                    [&set](const QString &name) { return set.contains(name); });
  qint64 setTime = timer.nsecsElapsed();

  qDebug() << "List hits:" << listHits << "Time (ns):" << listTime;
  qDebug() << "Set hits:" << setHits << "Time (ns):" << setTime;
  qDebug() << "Improvement:" << (double)listTime / setTime << "x";

  return 0;
}
