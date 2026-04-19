#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTest>
#include <QVector>

struct SessionData {
  QString id;
  QString title;
  QJsonObject rawObject;
};

class SessionModelPerformanceTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testSessionUpdatePerformance() {
    int count = 5000;

    // 1. Setup old way
    QJsonArray oldSessions;
    for (int i = 0; i < count; ++i) {
      QJsonObject obj;
      obj.insert(QStringLiteral("id"), QStringLiteral("id_%1").arg(i));
      obj.insert(QStringLiteral("title"), QStringLiteral("Title %1").arg(i));
      oldSessions.append(obj);
    }

    // Benchmark old way
    QElapsedTimer timer;
    timer.start();
    int oldWayHits = 0;
    for (int i = 0; i < count; ++i) {
      QString idToFind = QStringLiteral("id_%1").arg(i);
      for (int j = 0; j < oldSessions.size(); ++j) {
        if (oldSessions[j].toObject().value(QStringLiteral("id")).toString() ==
            idToFind) {
          oldWayHits++;
          break;
        }
      }
    }
    qint64 oldTime = timer.nsecsElapsed();

    // 2. Setup combined way (struct + QHash)
    QVector<SessionData> newSessions;
    QHash<QString, int> idToIndex;
    newSessions.reserve(count);
    for (int i = 0; i < count; ++i) {
      QString id = QStringLiteral("id_%1").arg(i);
      SessionData data;
      data.id = id;
      data.title = QStringLiteral("Title %1").arg(i);
      QJsonObject obj;
      obj.insert(QStringLiteral("id"), data.id);
      obj.insert(QStringLiteral("title"), data.title);
      data.rawObject = obj;
      newSessions.append(data);
      idToIndex.insert(id, i);
    }

    // Benchmark combined way
    timer.start();
    int newWayHits = 0;
    for (int i = 0; i < count; ++i) {
      QString idToFind = QStringLiteral("id_%1").arg(i);
      if (idToIndex.contains(idToFind)) {
        int idx = idToIndex.value(idToFind);
        if (newSessions[idx].id == idToFind) {
          newWayHits++;
        }
      }
    }
    qint64 newTime = timer.nsecsElapsed();

    qDebug() << "Old way hits:" << oldWayHits << "Time (ns):" << oldTime;
    qDebug() << "New way hits:" << newWayHits << "Time (ns):" << newTime;
    qDebug() << "Improvement:" << (double)oldTime / newTime << "x";

    QCOMPARE(oldWayHits, newWayHits);
  }
};

QTEST_MAIN(SessionModelPerformanceTest)
#include "bench_sessionmodel.moc"
