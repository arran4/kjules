#include "../src/apimanager.h"
#include "../src/errorsmodel.h"
#include <QDateTime>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

// We can test APIManager directly without launching MainWindow.
// "At minimum, add APIManager-level tests showing:
// - a simulated failure from reloadSession(id, true) emits the background context as true;
// - the equivalent foreground call emits false;"

class TestBackgroundErrors : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testApiManagerBackgroundContext() {
    APIManager apiManager;

    // We intentionally don't set a valid token so it fails immediately.
    QSignalSpy errorSpy(&apiManager, &APIManager::errorOccurred);
    QSignalSpy reloadSpy(&apiManager, &APIManager::sessionReloadFailed);

    // Foreground call
    apiManager.reloadSession(QStringLiteral("invalid_id"), false);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    // Check errorOccurred signature is void errorOccurred(const QString &message, bool isBackground)
    QVariantList errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), false);

    QVariantList reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), false);

    // Background call
    apiManager.reloadSession(QStringLiteral("invalid_id"), true);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(reloadSpy.count(), 1);

    errorArgs = errorSpy.takeFirst();
    QCOMPARE(errorArgs.at(1).toBool(), true);

    reloadArgs = reloadSpy.takeFirst();
    QCOMPARE(reloadArgs.at(2).toBool(), true);
  }
};

QTEST_MAIN(TestBackgroundErrors)
#include "test_background_errors.moc"
