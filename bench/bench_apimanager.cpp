#include "../src/apimanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTest>
#include <QThread>
#include <atomic>

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
      QJsonObject req;
      req[QStringLiteral("source")] = source;
      req[QStringLiteral("prompt")] = QStringLiteral("Test prompt");
      req[QStringLiteral("automationMode")] = QStringLiteral("auto");
      manager.createSessionAsync(req);
    }
    qint64 oldTime = timer.nsecsElapsed();

    // We removed createSessions, so just leave this.
    qDebug() << "Old loop time (ns):" << oldTime;
  }

  void testFileCacheWriteSyncVsAsync() {
    QJsonArray cachedSessions;
    for (int i = 0; i < 1000; ++i) {
      QJsonObject sessionObj;
      sessionObj[QStringLiteral("id")] =
          QStringLiteral("session_") + QString::number(i);
      sessionObj[QStringLiteral("sourceContext")] =
          QJsonObject{{QStringLiteral("info"), QStringLiteral("dummy data")}};
      cachedSessions.append(sessionObj);
    }

    QString path = QDir::currentPath() + QStringLiteral("/test_cache");
    QDir dir(path);
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }

    QString filePath = path + QStringLiteral("/cached_sessions.json");

    QElapsedTimer timer;

    // Test Sync Write (simulating main thread blocking)
    timer.start();
    for (int i = 0; i < 50; ++i) {
      QFile file(filePath);
      if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        QJsonDocument writeDoc(cachedSessions);
        file.write(writeDoc.toJson());
        file.close();
      }
    }
    qint64 elapsedSync = timer.nsecsElapsed();

    // Test Async Write (offloaded to thread)
    timer.start();
    std::atomic<int> finishedCount{0};
    int startedCount = 0;
    for (int i = 0; i < 50; ++i) {
      QByteArray jsonBytes = QJsonDocument(cachedSessions).toJson();
      QString threadFilePath = filePath + QString::number(i);
      QThread *thread =
          QThread::create([threadFilePath, jsonBytes, &finishedCount]() {
            QFile file(threadFilePath);
            if (file.open(QIODevice::WriteOnly)) {
              file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
              file.write(jsonBytes);
              file.close();
            }
            finishedCount++;
          });
      if (thread) {
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
        startedCount++;
      }
    }

    // Wait for all threads to finish just to ensure clean exit for the test,
    // but the main thread elapsed time is what matters.
    qint64 elapsedAsyncMainThread = timer.nsecsElapsed();

    while (finishedCount < startedCount) {
      QCoreApplication::processEvents();
    }

    qDebug() << "Sync Write - Total Time for 50 writes (ns):" << elapsedSync;
    qDebug() << "Sync Write - Average time per write (ms):"
             << (elapsedSync / 50.0) / 1e6;

    qDebug() << "Async Write - Total Main Thread Time for 50 offloads (ns):"
             << elapsedAsyncMainThread;
    qDebug() << "Async Write - Average main thread time per offload (ms):"
             << (elapsedAsyncMainThread / 50.0) / 1e6;

    // Verification
    QVERIFY(elapsedAsyncMainThread < elapsedSync);
  }
};

QTEST_MAIN(APIManagerPerfTest)
#include "bench_apimanager.moc"
