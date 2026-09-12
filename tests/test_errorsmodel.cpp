#include "../src/api/apierror.h"
#include "../src/errorsmodel.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class ErrorsModelTest : public QObject {
  Q_OBJECT

private Q_SLOTS:

  void testTrimming() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");

    // Create an oversized file
    QJsonArray array;
    for (int i = 0; i < 250; ++i) {
      QJsonObject obj;
      obj[QStringLiteral("message")] = QString(QStringLiteral("Error %1")).arg(i);
      array.append(obj);
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
      QJsonDocument doc(array);
      file.write(doc.toJson());
      file.close();
    }

    // Load it
    ErrorsModel model(nullptr, path);
    QCOMPARE(model.rowCount(), 200);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Error 0"));
    QCOMPARE(model.data(model.index(199, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Error 199"));

    // Check if it was saved
    QFile file2(path);
    if (file2.open(QIODevice::ReadOnly)) {
      QJsonDocument doc = QJsonDocument::fromJson(file2.readAll());
      QCOMPARE(doc.array().size(), 200);
    } else {
      QFAIL("File not saved");
    }
  }

  void testMax200() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    for (int i = 0; i < 210; ++i) {
      QJsonObject obj;
      obj[QStringLiteral("message")] = QString(QStringLiteral("Error %1")).arg(i);
      model.addErrorObj(obj);
    }

    QCOMPARE(model.rowCount(), 200);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Error 209"));
    QCOMPARE(model.data(model.index(199, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Error 10"));
  }

  void testUnseenCount() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    QCOMPARE(model.unseenCount(), 0);

    QJsonObject obj;
    obj[QStringLiteral("message")] = QStringLiteral("Test");
    model.addErrorObj(obj);

    QCOMPARE(model.unseenCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::UnseenRole).toBool(), true);

    model.markSeen(0);
    QCOMPARE(model.unseenCount(), 0);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::UnseenRole).toBool(), false);
  }

  void testJobErrorsSyncAndIdempotency() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    // Add 1 operational error
    QJsonObject opErr;
    opErr[QStringLiteral("message")] = QStringLiteral("Network failure");
    model.addErrorObj(opErr);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.unseenCount(), 1);

    // Sync 2 job errors
    QJsonArray jobErrors;
    QJsonObject jErr1;
    jErr1[QStringLiteral("jobId")] = QStringLiteral("job-1");
    jErr1[QStringLiteral("attemptId")] = QStringLiteral("att-1");
    jErr1[QStringLiteral("message")] = QStringLiteral("Build failure in job 1");
    jobErrors.append(jErr1);

    QJsonObject jErr2;
    jErr2[QStringLiteral("jobId")] = QStringLiteral("job-2");
    jErr2[QStringLiteral("attemptId")] = QStringLiteral("att-2");
    jErr2[QStringLiteral("message")] = QStringLiteral("Auth failure in job 2");
    jobErrors.append(jErr2);

    model.syncJobErrors(jobErrors);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.unseenCount(), 3);

    // Idempotency: calling syncJobErrors again must not duplicate errors
    model.syncJobErrors(jobErrors);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.unseenCount(), 3);

    // Mark jErr1 as seen (row 1)
    model.markSeen(1);
    QCOMPARE(model.unseenCount(), 2);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::SeenRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::UnseenRole).toBool(), false);

    // Re-syncing preserves seen state for jErr1
    model.syncJobErrors(jobErrors);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.unseenCount(), 2);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::SeenRole).toBool(), true);

    // Disk isolation: errors.json only contains operational errors
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QCOMPARE(doc.array().size(), 1);
    QCOMPARE(doc.array().at(0).toObject().value(QStringLiteral("message")).toString(),
             QStringLiteral("Network failure"));

    // User dismisses job error jErr2 (row 2)
    model.removeError(2);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.unseenCount(), 1); // Only operational error left unseen

    // Re-syncing does not resurrect dismissed job error
    model.syncJobErrors(jobErrors);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.unseenCount(), 1);
  }
};

QTEST_MAIN(ErrorsModelTest)
#include "test_errorsmodel.moc"
