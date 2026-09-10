#include "../src/apimanager.h"
#include "../src/jobstore.h"
#include "../src/sessionwindow.h"
#include <QSignalSpy>
#include <QTest>

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

    JobAttemptData att2;
    att2.id = QStringLiteral("att2");
    att2.julesSessionId = QStringLiteral("sess2");

    job.attempts = {att1, att2};
    job.acceptedAttemptId = QStringLiteral("att2");
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    QVERIFY(window.windowTitle().contains(QStringLiteral("att2"))); // Initializes to winner
  }
};

QTEST_MAIN(TestSessionWindow)
#include "test_sessionwindow.moc"
