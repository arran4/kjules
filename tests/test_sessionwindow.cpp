
#include <QTest>
#include <QSignalSpy>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include "../src/sessionwindow.h"
#include "../src/jobstore.h"
#include "../src/apimanager.h"

class TestSessionWindow : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void testZeroAttempts() {
        JobStore store;
        JobData job;
        job.id = QStringLiteral("job_zero");
        job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Zero Job");
        store.addJob(job);

        SessionWindow window(job.id, &store, nullptr);

        auto *stack = window.findChild<QStackedWidget*>();
        QVERIFY(stack != nullptr);

        auto *list = window.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        QVERIFY(list->isHidden()); // Empty list is hidden

        QVERIFY(window.windowTitle().contains(QStringLiteral("Zero Job")));
    }

    void testMultipleAttempts() {
        JobStore store;
        JobData job;
        job.id = QStringLiteral("job_multi");
        job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Multi Job");

        JobAttemptData att1;
        att1.id = QStringLiteral("att1");
        att1.julesSessionId = QStringLiteral("sess1");
        att1.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Prompt 1");

        JobAttemptData att2;
        att2.id = QStringLiteral("att2");
        att2.julesSessionId = QStringLiteral("sess2");
        att2.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Prompt 2");

        job.attempts = {att1, att2};
        job.acceptedAttemptId = QStringLiteral("att2");
        store.addJob(job);

        SessionWindow window(job.id, &store, nullptr);

        auto *list = window.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        QVERIFY(!list->isHidden()); // Should be visible
        QCOMPARE(list->count(), 2);

        QVERIFY(window.windowTitle().contains(QStringLiteral("att2"))); // Initializes to winner
    }

    void testActionSignals() {
        JobStore store;
        JobData job;
        job.id = QStringLiteral("job_actions");

        JobAttemptData att1;
        att1.id = QStringLiteral("att1");
        job.attempts = {att1};
        store.addJob(job);

        SessionWindow window(job.id, &store, nullptr);

        QSignalSpy spyAttempt(&window, &SessionWindow::newAttemptRequested);
        QSignalSpy spyVariant(&window, &SessionWindow::variantRequested);
        QSignalSpy spyJobFrom(&window, &SessionWindow::newJobFromRequested);

        auto actions = window.findChildren<QAction*>();
        QAction* attemptAction = nullptr;
        QAction* variantAction = nullptr;
        QAction* newJobAction = nullptr;

        for (auto *a : actions) {
            if (a->text() == QStringLiteral("Launch New Attempt")) attemptAction = a;
            if (a->text() == QStringLiteral("Launch Variant...")) variantAction = a;
            if (a->text() == QStringLiteral("New Job From This...")) newJobAction = a;
        }

        QVERIFY(attemptAction != nullptr);
        QVERIFY(variantAction != nullptr);
        QVERIFY(newJobAction != nullptr);

        attemptAction->trigger();
        QCOMPARE(spyAttempt.count(), 1);
        QCOMPARE(spyAttempt.at(0).at(0).toString(), QStringLiteral("job_actions"));

        variantAction->trigger();
        QCOMPARE(spyVariant.count(), 1);

        newJobAction->trigger();
        QCOMPARE(spyJobFrom.count(), 1);
    }
};

QTEST_MAIN(TestSessionWindow)
#include "test_sessionwindow.moc"
