#include <QTest>
#include <QSignalSpy>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QStandardPaths>

#include "../src/mainwindow.h"
#include "../src/jobstore.h"
#include "../src/queuemodel.h"
#include "../src/errorsmodel.h"
#include "../src/migrationorchestrator.h"

class TestMigrationDecoupling : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
  }

  void testLegacyFilesNotWrittenAfterMigration() {
    QTemporaryDir dir;
    QDir::setCurrent(dir.path());

    // Write a dummy legacy file to trigger migration
    QFile qf(QStringLiteral("queue.json"));
    qf.open(QIODevice::WriteOnly);
    qf.write("{\"items\":[{\"requestData\":{\"prompt\":\"legacy\"}}]}");
    qf.close();

    // mock migration success marker
    MigrationOrchestrator::getMigratedFlag() = true;

    // Snapshot legacy modification time or content
    QFile qf_check(QStringLiteral("queue.json"));
    qf_check.open(QIODevice::ReadOnly);
    QByteArray initialLegacyData = qf_check.readAll();
    qf_check.close();

    // Now instantiate models and perform operations
    QueueModel queueModel(nullptr);
    ErrorsModel errorsModel(nullptr);

    // Mutate queue which calls save()
    QueueItem item;
    item.requestData = QJsonObject{{QStringLiteral("prompt"), QStringLiteral("new_item")}};
    queueModel.enqueueItem(item);

    // Mutate errors which calls saveErrors()
    errorsModel.addErrorObj(QJsonObject{{QStringLiteral("message"), QStringLiteral("new_error")}});

    // Verify legacy files are unchanged
    qf_check.open(QIODevice::ReadOnly);
    QByteArray finalLegacyData = qf_check.readAll();
    qf_check.close();

    QCOMPARE(initialLegacyData, finalLegacyData);

    // Verify errors.json wasn't suddenly created
    QVERIFY(!QFile::exists(QStringLiteral("errors.json")));
  }
};

QTEST_MAIN(TestMigrationDecoupling)
#include "test_migration_decoupling.moc"
