#include "../src/api/apierror.h"
#include "../src/errorsmodel.h"
#include "../src/migrationorchestrator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class ErrorsModelTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init() { MigrationOrchestrator::getMigratedFlag() = false; }

  void cleanup() { MigrationOrchestrator::getMigratedFlag() = false; }

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
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::SeenRole).toBool(), false);

    model.markSeen(0);
    QCOMPARE(model.unseenCount(), 0);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::UnseenRole).toBool(), false);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::SeenRole).toBool(), true);
  }

  void testLoadedErrorsStartSeen() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");

    QJsonArray array;
    QJsonObject obj1{{QStringLiteral("message"), QStringLiteral("Persisted Error 1")}};
    QJsonObject obj2{{QStringLiteral("message"), QStringLiteral("Persisted Error 2")}};
    array.append(obj1);
    array.append(obj2);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(array).toJson());
    file.close();

    ErrorsModel model(nullptr, path);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.unseenCount(), 0);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::SeenRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::UnseenRole).toBool(), false);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::SeenRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::UnseenRole).toBool(), false);

    // Adding a new error at runtime makes only the new error unseen
    QJsonObject newObj{{QStringLiteral("message"), QStringLiteral("New Error")}};
    model.addErrorObj(newObj);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.unseenCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::UnseenRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), ErrorsModel::UnseenRole).toBool(), false);
  }

  void testMetadataRoles() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    QJsonObject obj;
    obj[QStringLiteral("message")] = QStringLiteral("API timeout");
    obj[QStringLiteral("sourceId")] = QStringLiteral("sources/github/foo/bar");
    obj[QStringLiteral("sessionId")] = QStringLiteral("sess_123");
    obj[QStringLiteral("operation")] = QStringLiteral("createSession");
    obj[QStringLiteral("provider")] = QStringLiteral("github");
    obj[QStringLiteral("jobId")] = QStringLiteral("job_abc");
    obj[QStringLiteral("attemptId")] = QStringLiteral("att_xyz");
    obj[QStringLiteral("httpDetails")] = QStringLiteral("504 Gateway Timeout");
    obj[QStringLiteral("timestamp")] = QStringLiteral("2026-09-27T04:00:00Z");
    QJsonObject reqObj{{QStringLiteral("endpoint"), QStringLiteral("/api/v1")}};
    QJsonObject respObj{{QStringLiteral("status"), 504}};
    obj[QStringLiteral("request")] = reqObj;
    obj[QStringLiteral("response")] = respObj;

    model.addErrorObj(obj);

    QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, ErrorsModel::MessageRole).toString(), QStringLiteral("API timeout"));
    QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QStringLiteral("API timeout"));
    QCOMPARE(model.data(idx, ErrorsModel::SourceIdRole).toString(), QStringLiteral("sources/github/foo/bar"));
    QCOMPARE(model.data(idx, ErrorsModel::SessionIdRole).toString(), QStringLiteral("sess_123"));
    QCOMPARE(model.data(idx, ErrorsModel::OperationRole).toString(), QStringLiteral("createSession"));
    QCOMPARE(model.data(idx, ErrorsModel::ProviderRole).toString(), QStringLiteral("github"));
    QCOMPARE(model.data(idx, ErrorsModel::JobIdRole).toString(), QStringLiteral("job_abc"));
    QCOMPARE(model.data(idx, ErrorsModel::AttemptIdRole).toString(), QStringLiteral("att_xyz"));
    QCOMPARE(model.data(idx, ErrorsModel::HttpDetailsRole).toString(), QStringLiteral("504 Gateway Timeout"));
    QCOMPARE(model.data(idx, ErrorsModel::RequestRole).toJsonObject(), reqObj);
    QCOMPARE(model.data(idx, ErrorsModel::ResponseRole).toJsonObject(), respObj);
    QVERIFY(!model.data(idx, ErrorsModel::TimestampRole).toString().isEmpty());
  }

  void testMarkAllSeen() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Err 1")}});
    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Err 2")}});
    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Err 3")}});

    QCOMPARE(model.unseenCount(), 3);
    model.markAllSeen();
    QCOMPARE(model.unseenCount(), 0);
    for (int i = 0; i < model.rowCount(); ++i) {
      QCOMPARE(model.data(model.index(i, 0), ErrorsModel::UnseenRole).toBool(), false);
      QCOMPARE(model.data(model.index(i, 0), ErrorsModel::SeenRole).toBool(), true);
    }
  }

  void testUpdateAndRemoveError() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel model(nullptr, path);

    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Initial 1")}});
    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Initial 2")}});

    QCOMPARE(model.rowCount(), 2);

    // Update row 0
    QJsonObject updated{{QStringLiteral("message"), QStringLiteral("Updated 2")}};
    model.updateError(0, updated);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Updated 2"));

    // Remove row 1
    model.removeError(1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ErrorsModel::MessageRole).toString(), QStringLiteral("Updated 2"));

    // Clear
    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.unseenCount(), 0);
  }

  void testMigrationGuard() {
    QTemporaryDir dir;
    QString path = dir.path() + QStringLiteral("/errors.json");

    // Write initial file
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[]");
    file.close();

    // Enable migrated flag
    MigrationOrchestrator::getMigratedFlag() = true;

    ErrorsModel model(nullptr, path);
    // Add error while migrated
    model.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("Should not save to file")}});
    QCOMPARE(model.rowCount(), 1);

    // Verify file on disk was not written
    QFile readFile(path);
    QVERIFY(readFile.open(QIODevice::ReadOnly));
    QCOMPARE(readFile.readAll(), QByteArray("[]"));
    readFile.close();
  }
};

QTEST_MAIN(ErrorsModelTest)
#include "test_errorsmodel.moc"
