#include <QCoreApplication>
#include <QStringList>
#include <QSet>
#include <QElapsedTimer>
#include <QDebug>
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
    int listHits = 0;
    for (const QString& name : testNames) {
        if (list.contains(name)) {
            listHits++;
        }
    }
    qint64 listTime = timer.nsecsElapsed();

    // Benchmark QSet
    timer.start();
    QSet<QString> set(list.begin(), list.end());
    int setHits = 0;
    for (const QString& name : testNames) {
        if (set.contains(name)) {
            setHits++;
        }
    }
    qint64 setTime = timer.nsecsElapsed();

    qDebug() << "List hits:" << listHits << "Time (ns):" << listTime;
    qDebug() << "Set hits:" << setHits << "Time (ns):" << setTime;
    qDebug() << "Improvement:" << (double)listTime / setTime << "x";

    return 0;
}
