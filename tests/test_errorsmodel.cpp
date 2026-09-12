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
};

QTEST_MAIN(ErrorsModelTest)
#include "test_errorsmodel.moc"
