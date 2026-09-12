
#include "../src/apimanager.h"
#include "../src/jobpolicy.h"
#include "../src/jobstore.h"
#include "../src/sessionwindow.h"
#include <QListWidget>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTest>
#include <QUuid>

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

    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack != nullptr);

    auto *list = window.findChild<QListWidget *>();
    QVERIFY(list != nullptr);
    QVERIFY(list->isHidden()); // Empty list is hidden

    QVERIFY(window.windowTitle().contains(QStringLiteral("Zero Job")));
  }

  void testOneAttempt() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_one");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("One Job");

    JobAttemptData att1;
    att1.id = QStringLiteral("att1");
    att1.julesSessionId = QStringLiteral("sess1");

    job.attempts = {att1};

    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);

    auto *list = window.findChild<QListWidget *>();
    QVERIFY(list != nullptr);
    QVERIFY(list->isHidden()); // Hidden for exactly 1 attempt

    QVERIFY(window.windowTitle().contains(QStringLiteral("att1"))); // Uses the only attempt
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

    auto *list = window.findChild<QListWidget *>();
    QVERIFY(list != nullptr);
    QVERIFY(!list->isHidden()); // Should be visible
    QCOMPARE(list->count(), 2);

    QVERIFY(window.windowTitle().contains(QStringLiteral("att2"))); // Initializes to winner
  }

  void testActionSignals() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_actions");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Canonical Title");

    JobAttemptData att1;
    att1.id = QStringLiteral("att1");
    att1.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Attempt Prompt");
    job.attempts = {att1};

    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);

    QSignalSpy spyAttempt(&window, &SessionWindow::newAttemptRequested);
    QSignalSpy spyVariant(&window, &SessionWindow::variantRequested);
    QSignalSpy spyRetry(&window, &SessionWindow::retryAttemptRequested);
    QSignalSpy spyJobFrom(&window, &SessionWindow::newJobFromRequested);

    auto actions = window.findChildren<QAction *>();
    QAction *attemptAction = nullptr;
    QAction *variantAction = nullptr;
    QAction *retryAction = nullptr;
    QAction *newJobAction = nullptr;

    for (auto *a : actions) {
      if (a->text() == QStringLiteral("Launch New Attempt"))
        attemptAction = a;
      if (a->text() == QStringLiteral("Launch Variant..."))
        variantAction = a;
      if (a->text() == QStringLiteral("Retry Failed Attempt"))
        retryAction = a;
      if (a->text() == QStringLiteral("New Job From This..."))
        newJobAction = a;
    }

    QVERIFY(attemptAction != nullptr);
    QVERIFY(variantAction != nullptr);
    QVERIFY(retryAction != nullptr);
    QVERIFY(newJobAction != nullptr);

    attemptAction->trigger();
    QCOMPARE(spyAttempt.count(), 1);
    QCOMPARE(spyAttempt.at(0).at(0).toString(), QStringLiteral("job_actions"));

    variantAction->trigger();
    QCOMPARE(spyVariant.count(), 1);
    QCOMPARE(spyVariant.at(0).at(0).toString(), QStringLiteral("job_actions"));
    QCOMPARE(spyVariant.at(0).at(1).toJsonObject().value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Attempt Prompt"));

    retryAction->trigger();
    QCOMPARE(spyRetry.count(), 1);
    QCOMPARE(spyRetry.at(0).at(0).toString(), QStringLiteral("job_actions"));
    QCOMPARE(spyRetry.at(0).at(1).toString(), QStringLiteral("att1"));

    newJobAction->trigger();
    QCOMPARE(spyJobFrom.count(), 1);
    QCOMPARE(spyJobFrom.at(0).at(0).toJsonObject().value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Attempt Prompt"));
  }

  void testVariantUsesImmutableSnapshot() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_snap");
    job.canonicalRequest[QStringLiteral("prompt")] = QStringLiteral("Original Canonical Prompt");
    job.canonicalRequest[QStringLiteral("source")] = QStringLiteral("sources/github/org/repo");

    JobAttemptData att1;
    att1.id = QStringLiteral("att1");
    att1.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Snapshot Attempt 1 Prompt");
    att1.requestSnapshot[QStringLiteral("source")] = QStringLiteral("sources/github/org/repo");
    job.attempts.append(att1);

    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    QSignalSpy spyVariant(&window, &SessionWindow::variantRequested);

    auto actions = window.findChildren<QAction *>();
    QAction *variantAction = nullptr;
    for (auto *a : actions) {
      if (a->text() == QStringLiteral("Launch Variant...")) {
        variantAction = a;
        break;
      }
    }
    QVERIFY(variantAction != nullptr);

    variantAction->trigger();
    QCOMPARE(spyVariant.count(), 1);
    QCOMPARE(spyVariant.at(0).at(0).toString(), QStringLiteral("job_snap"));
    // Must use attempt's immutable request snapshot, NOT the canonical prompt
    QCOMPARE(spyVariant.at(0).at(1).toJsonObject().value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Snapshot Attempt 1 Prompt"));
  }

  void testChooseWinnerSavesToStoreAndEmitsSignal() {
    QTemporaryDir dir;
    QString storePath = dir.path() + QStringLiteral("/jobs.json");
    JobStore store(storePath);

    JobData job;
    job.id = QStringLiteral("job_winner");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Winner Test");

    JobAttemptData att1;
    att1.id = QStringLiteral("att1");
    att1.julesState = QStringLiteral("COMPLETED");

    JobAttemptData att2;
    att2.id = QStringLiteral("att2");
    att2.julesState = QStringLiteral("COMPLETED");

    job.attempts = {att1, att2};
    store.addJob(job);
    QVERIFY(store.save());

    SessionWindow window(job.id, &store, nullptr);
    QSignalSpy spyMutated(&window, &SessionWindow::jobMutated);

    auto actions = window.findChildren<QAction *>();
    QAction *winnerAction = nullptr;
    for (auto *a : actions) {
      if (a->text() == QStringLiteral("Choose as Winner")) {
        winnerAction = a;
        break;
      }
    }
    QVERIFY(winnerAction != nullptr);

    // Initial selected attempt is att1
    winnerAction->trigger();
    QCOMPARE(spyMutated.count(), 1);
    QCOMPARE(spyMutated.at(0).at(0).toString(), QStringLiteral("job_winner"));
    QCOMPARE(store.getJobById(QStringLiteral("job_winner"))->acceptedAttemptId, QStringLiteral("att1"));

    // Verify persisted on disk
    JobStore verifyStore(storePath);
    QVERIFY(verifyStore.load());
    QCOMPARE(verifyStore.getJobById(QStringLiteral("job_winner"))->acceptedAttemptId, QStringLiteral("att1"));

    // Switch selection to att2 in list widget
    auto *list = window.findChild<QListWidget *>();
    QVERIFY(list != nullptr);
    list->setCurrentRow(1);

    winnerAction->trigger();
    QCOMPARE(spyMutated.count(), 2);
    QCOMPARE(store.getJobById(QStringLiteral("job_winner"))->acceptedAttemptId, QStringLiteral("att2"));

    // Verify updated on disk
    QVERIFY(verifyStore.load());
    QCOMPARE(verifyStore.getJobById(QStringLiteral("job_winner"))->acceptedAttemptId, QStringLiteral("att2"));
  }

  void testZeroAttemptJobCanBeOpenedAndRelaunched() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_zero_relaunch");
    job.canonicalRequest[QStringLiteral("prompt")] = QStringLiteral("Zero attempt prompt");
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    QSignalSpy spyNewAttempt(&window, &SessionWindow::newAttemptRequested);

    auto actions = window.findChildren<QAction *>();
    QAction *attemptAction = nullptr;
    for (auto *a : actions) {
      if (a->text() == QStringLiteral("Launch New Attempt")) {
        attemptAction = a;
        break;
      }
    }
    QVERIFY(attemptAction != nullptr);
    attemptAction->trigger();
    QCOMPARE(spyNewAttempt.count(), 1);
    QCOMPARE(spyNewAttempt.at(0).at(0).toString(), QStringLiteral("job_zero_relaunch"));
  }

  void testMultipleAttemptsIndependentlySelectable() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_selectable");

    JobAttemptData att1;
    att1.id = QStringLiteral("att_alpha");
    att1.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Alpha Prompt");
    att1.julesState = QStringLiteral("IN_PROGRESS");

    JobAttemptData att2;
    att2.id = QStringLiteral("att_beta");
    att2.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Beta Prompt");
    att2.julesState = QStringLiteral("COMPLETED");

    job.attempts = {att1, att2};
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    auto *list = window.findChild<QListWidget *>();
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 2);

    // Initial attempt is att1
    list->setCurrentRow(0);
    QCOMPARE(window.currentVariantRequest().value(QStringLiteral("prompt")).toString(), QStringLiteral("Alpha Prompt"));

    // Select att2
    list->setCurrentRow(1);
    QCOMPARE(window.currentVariantRequest().value(QStringLiteral("prompt")).toString(), QStringLiteral("Beta Prompt"));
  }

  void testNewJobFromCreatesIndependentIdentity() {
    JobStore store;
    JobData originalJob;
    originalJob.id = QStringLiteral("job_original_ident");

    JobAttemptData att;
    att.id = QStringLiteral("att_orig");
    att.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Original prompt");
    originalJob.attempts.append(att);
    store.addJob(originalJob);

    SessionWindow window(originalJob.id, &store, nullptr);
    QSignalSpy spyJobFrom(&window, &SessionWindow::newJobFromRequested);

    auto actions = window.findChildren<QAction *>();
    QAction *newJobAction = nullptr;
    for (auto *a : actions) {
      if (a->text() == QStringLiteral("New Job From This...")) {
        newJobAction = a;
        break;
      }
    }
    QVERIFY(newJobAction != nullptr);
    newJobAction->trigger();
    QCOMPARE(spyJobFrom.count(), 1);

    // Verify a new job created from this payload has a completely independent ID
    QJsonObject payload = spyJobFrom.at(0).at(0).toJsonObject();
    JobData newJob;
    newJob.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newJob.canonicalRequest = payload;
    store.addJob(newJob);

    QVERIFY(newJob.id != originalJob.id);
    QCOMPARE(store.jobs().size(), 2);
    QCOMPARE(store.getJobById(originalJob.id)->id, QStringLiteral("job_original_ident"));
    QCOMPARE(store.getJobById(newJob.id)->canonicalRequest.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Original prompt"));
  }

  void testRetryPreservesFailedAttemptAndCreatesAnotherAttempt() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_retry_test");

    JobAttemptData failedAtt;
    failedAtt.id = QStringLiteral("att_failed");
    failedAtt.dispatchState = QStringLiteral("FAILED");
    QJsonObject err;
    err[QStringLiteral("message")] = QStringLiteral("Compile error");
    failedAtt.launchErrors.append(err);
    job.attempts.append(failedAtt);
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    QSignalSpy spyRetry(&window, &SessionWindow::retryAttemptRequested);

    auto actions = window.findChildren<QAction *>();
    QAction *retryAction = nullptr;
    for (auto *a : actions) {
      if (a->text() == QStringLiteral("Retry Failed Attempt")) {
        retryAction = a;
        break;
      }
    }
    QVERIFY(retryAction != nullptr);
    retryAction->trigger();
    QCOMPARE(spyRetry.count(), 1);
    QCOMPARE(spyRetry.at(0).at(0).toString(), QStringLiteral("job_retry_test"));
    QCOMPARE(spyRetry.at(0).at(1).toString(), QStringLiteral("att_failed"));

    // Simulating retry dispatch creating a new attempt:
    JobAttemptData retriedAtt;
    retriedAtt.id = QStringLiteral("att_retry_success");
    retriedAtt.julesState = QStringLiteral("IN_PROGRESS");
    job.attempts.append(retriedAtt);
    store.updateJob(job);

    // Failed attempt MUST be preserved alongside the new attempt
    JobData *updatedJob = store.getJobById(QStringLiteral("job_retry_test"));
    QCOMPARE(updatedJob->attempts.size(), 2);
    QCOMPARE(updatedJob->attempts[0].id, QStringLiteral("att_failed"));
    QCOMPARE(updatedJob->attempts[0].dispatchState, QStringLiteral("FAILED"));
    QCOMPARE(updatedJob->attempts[0].launchErrors.size(), 1);
    QCOMPARE(updatedJob->attempts[1].id, QStringLiteral("att_retry_success"));
  }

  void testArchiveRetainsFullHistory() {
    QTemporaryDir dir;
    QString storePath = dir.path() + QStringLiteral("/jobs.json");
    JobStore store(storePath);

    JobData job;
    job.id = QStringLiteral("job_archived_history");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Archive History Title");

    JobAttemptData att1;
    att1.id = QStringLiteral("att_hist_1");
    att1.julesState = QStringLiteral("COMPLETED");
    att1.requestSnapshot[QStringLiteral("snap")] = 1;
    QJsonObject prMeta;
    prMeta[QStringLiteral("url")] = QStringLiteral("https://github.com/org/repo/pull/123");
    att1.prMetadata = prMeta;

    JobAttemptData att2;
    att2.id = QStringLiteral("att_hist_2");
    att2.dispatchState = QStringLiteral("FAILED");
    QJsonObject err;
    err[QStringLiteral("message")] = QStringLiteral("Timed out");
    att2.launchErrors.append(err);

    job.attempts = {att1, att2};
    job.acceptedAttemptId = QStringLiteral("att_hist_1");
    job.lifecycleMetadata[QStringLiteral("state")] = QStringLiteral("archived");

    QVERIFY(store.addJobTransactional(job));

    // Reload from disk to verify full history retention
    JobStore reloaded(storePath);
    QVERIFY(reloaded.load());
    JobData *archived = reloaded.getJobById(QStringLiteral("job_archived_history"));
    QVERIFY(archived != nullptr);
    QCOMPARE(JobPolicy::aggregateState(*archived), JobPolicy::JobAggregateState::Archived);
    QCOMPARE(archived->acceptedAttemptId, QStringLiteral("att_hist_1"));
    QCOMPARE(archived->attempts.size(), 2);
    QCOMPARE(archived->attempts[0].requestSnapshot.value(QStringLiteral("snap")).toInt(), 1);
    QCOMPARE(archived->attempts[0].prMetadata.value(QStringLiteral("url")).toString(),
             QStringLiteral("https://github.com/org/repo/pull/123"));
    QCOMPARE(archived->attempts[1].launchErrors.size(), 1);
  }

  void testLegacySingleSessionBehaviorAvailable() {
    QJsonObject session;
    session[QStringLiteral("id")] = QStringLiteral("sess_legacy_single");
    session[QStringLiteral("title")] = QStringLiteral("Standalone Legacy Session");
    session[QStringLiteral("prompt")] = QStringLiteral("Legacy prompt");

    SessionWindow window(session, nullptr, nullptr, false);
    QVERIFY(window.windowTitle().contains(QStringLiteral("Standalone Legacy Session")));
    QCOMPARE(window.currentVariantRequest().value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Legacy prompt"));
  }
};

QTEST_MAIN(TestSessionWindow)
#include "test_sessionwindow.moc"
