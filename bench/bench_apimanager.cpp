#include "../src/apimanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTest>

class APIManagerPerfTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testSessionCreation() {
    APIManager manager;
    manager.setApiKey(QStringLiteral("dummy")); // To pass canConnect()

    QStringList sources;
    for (int i = 0; i < 10000; ++i) {
      sources.append(QStringLiteral("Source_%1").arg(i));
    }

    QElapsedTimer timer;
    timer.start();
    for (const QString &source : sources) {
      manager.createSession(source, QStringLiteral("Test prompt"),
                            QStringLiteral("auto"));
    }
    qint64 oldTime = timer.nsecsElapsed();

    timer.start();
    manager.createSessions(sources, QStringLiteral("Test prompt"),
                           QStringLiteral("auto"));
    qint64 newTime = timer.nsecsElapsed();

    qDebug() << "Old loop time (ns):" << oldTime;
    qDebug() << "New batch time (ns):" << newTime;
    qDebug() << "Improvement:" << (double)oldTime / newTime << "x";
  }
};

QTEST_MAIN(APIManagerPerfTest)
#include "bench_apimanager.moc"
