
#include "../src/apimanager.h"
#include "../src/clickableprogressbar.h"
#include "../src/errorsmodel.h"
#include "../src/jobpolicy.h"
#include "../src/jobstore.h"
#include "../src/mainwindow.h"
#include "../src/newsessiondialog.h"
#include "../src/queuemodel.h"
#include "../src/sessionrequestbuilder.h"
#include "../src/sessionwindow.h"
#include "../src/sourcemodel.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QUuid>

class MockSimpleNetworkReply : public QNetworkReply {
  Q_OBJECT
public:
  MockSimpleNetworkReply(const QNetworkRequest &request, const QByteArray &responseBody, int statusCode,
                         QObject *parent = nullptr)
      : QNetworkReply(parent), m_data(responseBody), m_pos(0) {
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::PostOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
    if (statusCode >= 400) {
      setError(QNetworkReply::UnknownServerError, QStringLiteral("Server error %1").arg(statusCode));
    }
    open(QIODevice::ReadOnly);
    QTimer::singleShot(0, this, [this]() {
      Q_EMIT readyRead();
      Q_EMIT finished();
    });
  }
  qint64 bytesAvailable() const override { return (m_data.size() - m_pos) + QNetworkReply::bytesAvailable(); }
  qint64 readData(char *data, qint64 maxlen) override {
    qint64 bytesToRead = qMin(maxlen, (qint64)(m_data.size() - m_pos));
    memcpy(data, m_data.constData() + m_pos, bytesToRead);
    m_pos += bytesToRead;
    return bytesToRead;
  }
  void abort() override { close(); }
  bool isSequential() const override { return true; }

private:
  QByteArray m_data;
  qint64 m_pos;
};

class MockCreateRepoAndSessionNetworkManager : public QNetworkAccessManager {
  Q_OBJECT
public:
  QByteArray repoResponse = "{}";
  int repoStatusCode = 200;
  bool interceptCreateRepo = false;
  int createRepoCount = 0;

  QByteArray listSourcesResponse = "{\"sources\":[]}";
  int listSourcesStatusCode = 200;
  bool interceptListSources = false;
  int listSourcesCount = 0;

  QByteArray createSessionResponse = "{}";
  int createSessionStatusCode = 200;
  bool interceptCreateSession = false;
  int createSessionCount = 0;

  QList<QByteArray> capturedSessionRequests;

  MockCreateRepoAndSessionNetworkManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}

protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override {
    QString path = request.url().path();

    QByteArray responseBody;
    int statusCode = 200;
    bool intercepted = false;

    if (interceptCreateRepo && path.contains(QStringLiteral("/repos"))) {
      responseBody = repoResponse;
      statusCode = repoStatusCode;
      intercepted = true;
      createRepoCount++;
    } else if (interceptListSources && path.contains(QStringLiteral("/sources"))) {
      responseBody = listSourcesResponse;
      statusCode = listSourcesStatusCode;
      intercepted = true;
      listSourcesCount++;
    } else if (interceptCreateSession && path.contains(QStringLiteral("/sessions"))) {
      responseBody = createSessionResponse;
      statusCode = createSessionStatusCode;
      intercepted = true;
      if (op == PostOperation) {
        createSessionCount++;
        if (outgoingData) {
          capturedSessionRequests.append(outgoingData->readAll());
        }
      }
    }

    if (intercepted) {
      return new MockSimpleNetworkReply(request, responseBody, statusCode, this);
    }
    return QNetworkAccessManager::createRequest(op, request, outgoingData);
  }
};

class TestSessionWindow : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("KDE_HOME_READONLY", "1");
    qputenv("CANBERRA_DRIVER", "null");
    qputenv("KNOTIFICATIONS_DEFAULT_BACKEND", "null");
    QStandardPaths::setTestModeEnabled(true);
  }

  void init() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(dataDir).removeRecursively();
    QDir().mkpath(dataDir);
  }

  void cleanupTestCase() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(dataDir).removeRecursively();
  }

  void testCanonicalPresentationAndRecovery_data() {
    QTest::addColumn<bool>("direct");
    QTest::newRow("queue") << false;
    QTest::newRow("direct") << true;
  }

  void testCanonicalPresentationAndRecovery() {
    QFETCH(bool, direct);
    const auto request = SessionRequestBuilder::buildSessionRequest(
        QStringLiteral("sources/github/edited/repo"), QStringLiteral("edited-branch"),
        QStringLiteral("Canonical prompt"), QStringLiteral("AUTO_CREATE_PR"), true, true, 9, QStringLiteral("queue"));
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    QueueItem item;
    item.jobId = QStringLiteral("recovered");
    item.requestData = request;
    disconnect(window.apiManager(), &APIManager::sessionCreationFailed, &window, nullptr);
    if (direct) {
      window.sendItemNow(item, -1, false);
    } else {
      window.queueModel()->enqueueItem(item);
      QVERIFY(window.processQueueForTest());
    }
    auto *job = window.jobStore()->getJobById(item.jobId);
    QVERIFY(job);
    QCOMPARE(job->source, QStringLiteral("sources/github/edited/repo"));
    QCOMPARE(job->startingBranch, QStringLiteral("edited-branch"));
    QCOMPARE(job->automationMode, QStringLiteral("AUTO_CREATE_PR"));
    QVERIFY(job->planApproval);
    QVERIFY(job->ignoreConcurrency);
    QCOMPARE(job->priority, 9);
    QCOMPARE(job->canonicalRequest, request);
    QCOMPARE(job->attempts.size(), 1);
    SessionWindow selected(job->id, window.jobStore(), nullptr);
    bool rendered = false;
    for (auto *browser : selected.findChildren<QTextBrowser *>())
      if (browser->toPlainText().contains(QStringLiteral("Starting Branch:")) &&
          browser->toPlainText().contains(job->source) && browser->toPlainText().contains(job->startingBranch))
        rendered = true;
    QVERIFY(rendered);

    JobStore zeroStore;
    auto zero = JobData::fromRequest(request);
    zero.id = QStringLiteral("zero-canonical");
    zeroStore.addJob(zero);
    SessionWindow zeroWindow(zero.id, &zeroStore, nullptr);
    QVERIFY(zeroWindow.findChild<QLabel *>(QStringLiteral("zeroSourceLabel"))->text().contains(zero.source));
    QVERIFY(zeroWindow.findChild<QLabel *>(QStringLiteral("zeroBranchLabel"))->text().contains(zero.startingBranch));
  }

  void testWinnerEligibility_data() {
    QTest::addColumn<QString>("state");
    QTest::addColumn<bool>("eligible");
    QTest::newRow("completed") << QStringLiteral("COMPLETED") << true;
    QTest::newRow("failed") << QStringLiteral("FAILED") << false;
    QTest::newRow("active") << QStringLiteral("IN_PROGRESS") << false;
    QTest::newRow("cancelled") << QStringLiteral("CANCELED") << false;
  }

  void testWinnerEligibility() {
    QFETCH(QString, state);
    QFETCH(bool, eligible);
    JobStore store;
    JobData job;
    job.id = QStringLiteral("winner-eligibility");
    JobAttemptData attempt;
    attempt.id = QStringLiteral("accepted");
    attempt.julesState = state;
    job.attempts.append(attempt);
    job.acceptedAttemptId = attempt.id;
    QCOMPARE(JobPolicy::aggregateState(job) == JobPolicy::JobAggregateState::WinnerSatisfied, eligible);
    store.addJob(job);
    SessionWindow window(job.id, &store, nullptr);
    QCOMPARE(window.actionCollection()->action(QStringLiteral("choose_winner"))->isEnabled(), eligible);
    JobAttemptData sibling;
    sibling.id = QStringLiteral("sibling");
    sibling.julesState = QStringLiteral("IN_PROGRESS");
    job.attempts.append(sibling);
    QCOMPARE(JobPolicy::aggregateState(job) == JobPolicy::JobAggregateState::WinnerWithActive, eligible);
  }

  void testCreateRepoWorkflow();
  void testCreateRepoWorkflowRepoFailure();
  void testCreateRepoWorkflowSourceRefreshRetry();
  void testCreateRepoWorkflowBackgroundClassification();
  void testProcessQueueIndependenceFromFollowingReloads();

  void testDurableZeroHistory() {
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    QMultiMap<QString, QString> sources;
    sources.insert(QStringLiteral("sources/github/org/repo"), QStringLiteral("main"));
    window.onSessionCreated(sources, QStringLiteral("History task"), QString(), false, false, 0,
                            QStringLiteral("queue"));
    QCOMPARE(window.jobStore()->jobs().size(), 1);
    const auto id = window.jobStore()->jobs().first().id;
    window.onMoveToHoldingRequested(id);
    window.onMoveToQueueRequested(id);
    window.onMoveToHoldingRequested(id);
    window.onSessionCreatedResult(false, id, QString(), QJsonObject(),
                                  ApiError(ApiError::Type::Unknown, QStringLiteral("First launch error")));
    window.onSessionCreatedResult(false, id, QString(), QJsonObject(),
                                  ApiError(ApiError::Type::Unknown, QStringLiteral("Second launch error")));
    const auto job = *window.jobStore()->getJobById(id);
    JobStore reloaded;
    QVERIFY(reloaded.load());
    const auto history = reloaded.getJobById(id)->lifecycleMetadata.value(QStringLiteral("history")).toArray();
    QCOMPARE(history.size(), 6);
    QCOMPARE(history, job.lifecycleMetadata.value(QStringLiteral("history")).toArray());
    QCOMPARE(history[1].toObject().value(QStringLiteral("event")).toString(), QStringLiteral("holding"));
    QCOMPARE(history[2].toObject().value(QStringLiteral("event")).toString(), QStringLiteral("released"));
    SessionWindow sw(id, &reloaded, nullptr);
    const auto text = sw.findChild<QLabel *>(QStringLiteral("zeroHistoryLabel"))->text();
    QVERIFY(text.contains(QStringLiteral("First launch error")));
    QVERIFY(text.indexOf(QStringLiteral("First launch error")) < text.indexOf(QStringLiteral("Second launch error")));
  }

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

  void testDiagnosticIdentityAndAttemptFiltering() {
    QTemporaryDir dir;
    QString errPath = dir.path() + QStringLiteral("/errors.json");
    ErrorsModel errorsModel(nullptr, errPath);

    // 1. Unrelated operational diagnostic (no jobId, sessionId = "sess_other")
    QJsonObject opErr1;
    opErr1[QStringLiteral("message")] = QStringLiteral("Operational error other session");
    opErr1[QStringLiteral("sessionId")] = QStringLiteral("sess_other");
    errorsModel.addErrorObj(opErr1);

    // 2. Unrelated operational diagnostic (no jobId, no sessionId)
    QJsonObject opErr2;
    opErr2[QStringLiteral("message")] = QStringLiteral("Operational error no session");
    errorsModel.addErrorObj(opErr2);

    // Sync job errors from JobStore:
    // 3. Job diagnostic for job_1, attempt_1: FAILED dispatch with NO remote session ID
    QJsonObject jobErr1;
    jobErr1[QStringLiteral("message")] = QStringLiteral("Pre-launch validation failed");
    jobErr1[QStringLiteral("jobId")] = QStringLiteral("job_1");
    jobErr1[QStringLiteral("attemptId")] = QStringLiteral("att_1");
    // Notice: NO sessionId

    // 4. Job diagnostic for job_1, attempt_2: FAILED with remote session ID
    QJsonObject jobErr2;
    jobErr2[QStringLiteral("message")] = QStringLiteral("Jules remote execution error");
    jobErr2[QStringLiteral("jobId")] = QStringLiteral("job_1");
    jobErr2[QStringLiteral("attemptId")] = QStringLiteral("att_2");
    jobErr2[QStringLiteral("sessionId")] = QStringLiteral("remote_sess_2");

    // 5. Operational diagnostic tied to remote_sess_2 (e.g. from API manager during execution)
    QJsonObject opErr3;
    opErr3[QStringLiteral("message")] = QStringLiteral("API call error on remote session");
    opErr3[QStringLiteral("sessionId")] = QStringLiteral("remote_sess_2");
    errorsModel.addErrorObj(opErr3);

    // 6. Job diagnostic for another job
    QJsonObject jobErrOther;
    jobErrOther[QStringLiteral("message")] = QStringLiteral("Other job failure");
    jobErrOther[QStringLiteral("jobId")] = QStringLiteral("job_other");
    jobErrOther[QStringLiteral("attemptId")] = QStringLiteral("att_x");

    QJsonArray jobErrors{jobErr1, jobErr2, jobErrOther};
    errorsModel.syncJobErrors(jobErrors);

    // Test SessionErrorFilterProxyModel:
    SessionErrorFilterProxyModel proxy(QStringLiteral(""));
    proxy.setSourceModel(&errorsModel);

    // Target attempt 1 (job_1, att_1, NO remote session):
    proxy.setFilterTarget(QStringLiteral("job_1"), QStringLiteral("att_1"), QString());
    // Should accept ONLY jobErr1 (pre-launch failure with no remote session).
    // Sibling attempt_2, other job, and operational diagnostics must not leak!
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), ErrorsModel::MessageRole).toString(),
             QStringLiteral("Pre-launch validation failed"));

    // Switch selection to sibling attempt 2 (job_1, att_2, remote_sess_2):
    proxy.setFilterTarget(QStringLiteral("job_1"), QStringLiteral("att_2"), QStringLiteral("remote_sess_2"));
    // Should accept jobErr2 AND opErr3 (both associated with attempt 2 / remote_sess_2).
    // attempt 1 must disappear!
    QCOMPARE(proxy.rowCount(), 2);
    QStringList msgs;
    for (int i = 0; i < proxy.rowCount(); ++i) {
      msgs.append(proxy.data(proxy.index(i, 0), ErrorsModel::MessageRole).toString());
    }
    QVERIFY(msgs.contains(QStringLiteral("Jules remote execution error")));
    QVERIFY(msgs.contains(QStringLiteral("API call error on remote session")));
    QVERIFY(!msgs.contains(QStringLiteral("Pre-launch validation failed")));

    // Standalone legacy SessionWindow mode: filter by remote session ID
    SessionErrorFilterProxyModel legacyProxy(QStringLiteral("sess_other"));
    legacyProxy.setSourceModel(&errorsModel);
    QCOMPARE(legacyProxy.rowCount(), 1);
    QCOMPARE(legacyProxy.data(legacyProxy.index(0, 0), ErrorsModel::MessageRole).toString(),
             QStringLiteral("Operational error other session"));
  }

  void testAttemptTimestampsExposedAndVisible() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_timestamps");

    JobAttemptData att;
    att.id = QStringLiteral("att_time");
    att.createdAt = QDateTime::fromString(QStringLiteral("2026-09-10T10:15:00Z"), Qt::ISODate);
    att.updatedAt = QDateTime::fromString(QStringLiteral("2026-09-10T11:45:00Z"), Qt::ISODate);
    att.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Timestamp prompt");
    job.attempts.append(att);
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);
    QJsonObject sessionData = window.currentSessionData();

    // Verify fields are projected into currentSessionData
    QVERIFY(sessionData.contains(QStringLiteral("createTime")));
    QVERIFY(sessionData.contains(QStringLiteral("updateTime")));
    QCOMPARE(sessionData.value(QStringLiteral("createTime")).toString(), QStringLiteral("2026-09-10T10:15:00Z"));
    QCOMPARE(sessionData.value(QStringLiteral("updateTime")).toString(), QStringLiteral("2026-09-10T11:45:00Z"));

    // Verify details HTML contains the timestamps
    auto browsers = window.findChildren<QTextBrowser *>();
    bool foundDetails = false;
    for (auto *b : browsers) {
      if (b->toHtml().contains(QStringLiteral("Create Time:")) &&
          b->toHtml().contains(QStringLiteral("Update Time:"))) {
        foundDetails = true;
        break;
      }
    }
    QVERIFY(foundDetails);
  }

  void testZeroAttemptCompactStatusAndHistory() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_zero_history");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Zero History Title");
    job.canonicalRequest[QStringLiteral("prompt")] = QStringLiteral("Zero History Prompt");
    job.legacyMetadata[QStringLiteral("blocked")] = true;
    job.legacyMetadata[QStringLiteral("lastError")] = QStringLiteral("Failed to resolve branch before dispatch");
    job.createdAt = QDateTime::currentDateTimeUtc();
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);

    QLabel *titleLabel = window.findChild<QLabel *>(QStringLiteral("zeroTitleLabel"));
    QVERIFY(titleLabel != nullptr && titleLabel->text().contains(QStringLiteral("Zero History Title")));

    QLabel *idLabel = window.findChild<QLabel *>(QStringLiteral("zeroIdLabel"));
    QVERIFY(idLabel != nullptr && idLabel->text().contains(QStringLiteral("job_zero_history")));

    QLabel *schedLabel = window.findChild<QLabel *>(QStringLiteral("zeroSchedulingStateLabel"));
    QVERIFY(schedLabel != nullptr && schedLabel->text().contains(QStringLiteral("Blocked")));

    QLabel *reasonLabel = window.findChild<QLabel *>(QStringLiteral("zeroReasonLabel"));
    QVERIFY(reasonLabel != nullptr && reasonLabel->text().contains(QStringLiteral("concurrency")));

    QLabel *errorLabel = window.findChild<QLabel *>(QStringLiteral("zeroErrorLabel"));
    QVERIFY(errorLabel != nullptr && errorLabel->text().contains(QStringLiteral("Failed to resolve branch")));

    QLabel *timestampsLabel = window.findChild<QLabel *>(QStringLiteral("zeroTimestampsLabel"));
    QVERIFY(timestampsLabel != nullptr && timestampsLabel->text().contains(QStringLiteral("Created:")));

    QTextBrowser *promptBrowser = window.findChild<QTextBrowser *>(QStringLiteral("zeroPromptBrowser"));
    QVERIFY(promptBrowser != nullptr && promptBrowser->toPlainText() == QStringLiteral("Zero History Prompt"));

    QPushButton *launchBtn = window.findChild<QPushButton *>(QStringLiteral("zeroLaunchButton"));
    QVERIFY(launchBtn != nullptr && launchBtn->isEnabled());

    QPushButton *variantBtn = window.findChild<QPushButton *>(QStringLiteral("zeroVariantButton"));
    QVERIFY(variantBtn != nullptr && variantBtn->isEnabled());
  }

  void testArchivedJobActionConstraints() {
    JobStore store;
    JobData job;
    job.id = QStringLiteral("job_archived_actions");
    job.lifecycleMetadata[QStringLiteral("state")] = QStringLiteral("archived");

    JobAttemptData att;
    att.id = QStringLiteral("att_archived");
    job.attempts.append(att);
    store.addJob(job);

    SessionWindow window(job.id, &store, nullptr);

    // Mutating actions must be disabled
    QAction *newAttempt = window.actionCollection()->action(QStringLiteral("launch_new_attempt"));
    QVERIFY(newAttempt != nullptr && !newAttempt->isEnabled());

    QAction *variant = window.actionCollection()->action(QStringLiteral("launch_variant"));
    QVERIFY(variant != nullptr && !variant->isEnabled());

    QAction *retry = window.actionCollection()->action(QStringLiteral("retry_attempt"));
    QVERIFY(retry != nullptr && !retry->isEnabled());

    QAction *winner = window.actionCollection()->action(QStringLiteral("choose_winner"));
    QVERIFY(winner != nullptr && !winner->isEnabled());

    QAction *archive = window.actionCollection()->action(QStringLiteral("archive_job"));
    QVERIFY(archive != nullptr && !archive->isEnabled());

    // Explicit non-mutating or confirmed actions remain enabled
    QAction *newJobFrom = window.actionCollection()->action(QStringLiteral("new_job_from"));
    QVERIFY(newJobFrom != nullptr && newJobFrom->isEnabled());

    QAction *deleteJob = window.actionCollection()->action(QStringLiteral("delete_job"));
    QVERIFY(deleteJob != nullptr && deleteJob->isEnabled());
  }

  void testWorkflowNewJobFrom() {
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);

    JobStore *store = window.jobStore();
    QVERIFY(store != nullptr);

    JobData originalJob;
    originalJob.id = QStringLiteral("orig_job_workflow");
    originalJob.prompt = QStringLiteral("Original prompt");
    originalJob.source = QStringLiteral("sources/github/org/repo");
    originalJob.startingBranch = QStringLiteral("main");
    originalJob.canonicalRequest[QStringLiteral("prompt")] = originalJob.prompt;
    originalJob.canonicalRequest[QStringLiteral("source")] = originalJob.source;
    JobAttemptData origAtt;
    origAtt.id = QStringLiteral("orig_att");
    originalJob.attempts.append(origAtt);
    store->addJobTransactional(originalJob);

    QCOMPARE(store->jobs().size(), 1);

    SessionWindow sw(originalJob.id, store, window.apiManager(), window.errorsModel(), true, &window);
    window.connectSessionWindow(&sw);
    sw.actionCollection()->action(QStringLiteral("new_job_from"))->trigger();
    auto *dialog = window.findChild<NewSessionDialog *>();
    QVERIFY(dialog);
    QMultiMap<QString, QString> sources;
    sources.insert(QStringLiteral("sources/github/org/repo"), QStringLiteral("feature-branch"));
    Q_EMIT dialog->createSessionRequested(sources, QStringLiteral("New Independent Workflow Prompt"),
                                          QStringLiteral("AUTOMATION_MODE_ASAP"), false, false, 0,
                                          QStringLiteral("queue"));
    dialog->close();

    // Resulting job gets a distinct ID
    QCOMPARE(store->jobs().size(), 2);
    JobData *resultingJob = nullptr;
    for (const auto &j : store->jobs()) {
      if (j.id != QStringLiteral("orig_job_workflow")) {
        resultingJob = store->getJobById(j.id);
        break;
      }
    }
    QVERIFY(resultingJob != nullptr);
    QVERIFY(resultingJob->id != QStringLiteral("orig_job_workflow"));
    QCOMPARE(resultingJob->prompt, QStringLiteral("New Independent Workflow Prompt"));

    // Original job remains completely unchanged
    JobData *origCheck = store->getJobById(QStringLiteral("orig_job_workflow"));
    QVERIFY(origCheck != nullptr);
    QCOMPARE(origCheck->prompt, QStringLiteral("Original prompt"));
    QCOMPARE(origCheck->attempts.size(), 1);
    QCOMPARE(origCheck->attempts[0].id, QStringLiteral("orig_att"));
  }

  void testWorkflowRetry() {
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);

    JobStore *store = window.jobStore();
    QueueModel *queue = window.queueModel();
    QVERIFY(store != nullptr && queue != nullptr);

    JobData job;
    job.id = QStringLiteral("retry_workflow_job");
    JobAttemptData failedAtt;
    failedAtt.id = QStringLiteral("att_failed_orig");
    failedAtt.dispatchState = QStringLiteral("FAILED");
    failedAtt.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Retry snapshot prompt");
    failedAtt.requestSnapshot[QStringLiteral("source")] = QStringLiteral("sources/github/org/repo");
    job.attempts.append(failedAtt);
    store->addJobTransactional(job);

    SessionWindow sw(job.id, store, window.apiManager(), window.errorsModel(), true, &window);
    window.connectSessionWindow(&sw);

    QAction *retryAct = sw.actionCollection()->action(QStringLiteral("retry_attempt"));
    QVERIFY(retryAct != nullptr);
    retryAct->trigger();

    // Retry queues an item under the SAME Job ID with request matching failed snapshot
    QCOMPARE(queue->size(), 1);
    QueueItem qItem = queue->getItem(0);
    QCOMPARE(qItem.jobId, QStringLiteral("retry_workflow_job"));
    QCOMPARE(qItem.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Retry snapshot prompt"));

    // Processing queue creates a second JobAttemptData
    disconnect(window.apiManager(), &APIManager::sessionCreationFailed, &window, nullptr);
    window.processQueueForTest();

    JobData *updatedJob = store->getJobById(QStringLiteral("retry_workflow_job"));
    QVERIFY(updatedJob != nullptr);
    QCOMPARE(updatedJob->attempts.size(), 2);

    // Failed original attempt remains intact
    QCOMPARE(updatedJob->attempts[0].id, QStringLiteral("att_failed_orig"));
    QCOMPARE(updatedJob->attempts[0].dispatchState, QStringLiteral("FAILED"));

    // New attempt requestSnapshot matches what was actually dispatched
    QCOMPARE(updatedJob->attempts[1].requestSnapshot.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Retry snapshot prompt"));
    QVERIFY(updatedJob->attempts[1].id != QStringLiteral("att_failed_orig"));
  }

  void testWorkflowVariant_data() {
    QTest::addColumn<QString>("action");
    QTest::newRow("queue") << QStringLiteral("queue");
    QTest::newRow("send-next") << QStringLiteral("send_next");
    QTest::newRow("send-now") << QStringLiteral("send_now");
  }

  void testWorkflowVariant() {
    QFETCH(QString, action);
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);

    JobStore *store = window.jobStore();
    QueueModel *queue = window.queueModel();
    QVERIFY(store != nullptr && queue != nullptr);

    JobData job;
    job.id = QStringLiteral("variant_workflow_job");
    job.canonicalRequest =
        SessionRequestBuilder::buildSessionRequest(QStringLiteral("sources/original"), QStringLiteral("main"),
                                                   QStringLiteral("Default prompt"), QString(), false, false, 0);
    JobAttemptData att1;
    att1.id = QStringLiteral("att1_orig");
    att1.requestSnapshot[QStringLiteral("prompt")] = QStringLiteral("Original prompt");
    job.attempts.append(att1);
    store->addJobTransactional(job);

    // In variant flow: stays under the same Job, edited values pass through shared request builder
    QJsonObject editedRequest = SessionRequestBuilder::buildSessionRequest(
        QStringLiteral("sources/github/org/repo"), QStringLiteral("dev-branch"),
        QStringLiteral("Edited Variant Prompt"), QStringLiteral("AUTOMATION_MODE_ASAP"), true, true, 7, action);

    QMultiMap<QString, QString> sources;
    sources.insert(QStringLiteral("sources/github/org/repo"), QStringLiteral("dev-branch"));
    // Dispatch failure presentation is covered separately. Keep this coordinator
    // test focused on the request delivered to APIManager.
    disconnect(window.apiManager(), &APIManager::sessionCreationFailed, &window, nullptr);
    QSignalSpy dispatch(window.apiManager(), &APIManager::sessionCreationFailed);
    QueueItem sentinel;
    sentinel.jobId = QStringLiteral("other-job");
    sentinel.requestData[QStringLiteral("priority")] = 100;
    queue->enqueueItem(sentinel);
    window.submitVariantForJob(job.id, sources, QStringLiteral("Edited Variant Prompt"),
                               QStringLiteral("AUTOMATION_MODE_ASAP"), true, true, 7, action);
    QCOMPARE(store->getJobById(job.id)->canonicalRequest, job.canonicalRequest);
    if (action == QStringLiteral("send_now")) {
      QCOMPARE(queue->size(), 1);
    } else {
      const int row = action == QStringLiteral("send_next") ? 0 : 1;
      QCOMPARE(queue->size(), 2);
      QCOMPARE(queue->getItem(row).jobId, job.id);
      QCOMPARE(queue->getItem(row).requestData, editedRequest);
      window.syncModelsFromJobStore();
      QCOMPARE(queue->getItem(row).requestData, editedRequest);
      queue->removeItem(1 - row);
      QVERIFY(window.processQueueForTest());
    }
    QCOMPARE(dispatch.count(), 1);
    const auto dispatchedRequest = dispatch.first().at(2).toJsonObject();
    QCOMPARE(SessionRequestBuilder::createSession(dispatchedRequest),
             SessionRequestBuilder::createSession(editedRequest));

    JobData *updatedJob = store->getJobById(QStringLiteral("variant_workflow_job"));
    QVERIFY(updatedJob != nullptr);
    // Stays under the same Job
    QCOMPARE(updatedJob->id, QStringLiteral("variant_workflow_job"));
    QCOMPARE(updatedJob->attempts.size(), 2);

    // Resulting dispatched attempt has exact immutable distinct requestSnapshot
    QCOMPARE(updatedJob->attempts[0].requestSnapshot.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Original prompt"));
    QCOMPARE(updatedJob->attempts[1].requestSnapshot.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Edited Variant Prompt"));
    QCOMPARE(updatedJob->attempts[1].requestSnapshot.value(QStringLiteral("priority")).toInt(), 7);
    QCOMPARE(updatedJob->attempts[1].requestSnapshot, dispatchedRequest);
    JobStore reloaded;
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.getJobById(job.id)->attempts[1].requestSnapshot, dispatchedRequest);
  }

  void testQueueProjectionPreservesExplicitReorderAndDispatch() {
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    for (const auto &id : {QStringLiteral("first"), QStringLiteral("second")}) {
      auto job = JobData::fromRequest(SessionRequestBuilder::buildSessionRequest(
          QStringLiteral("sources/repo"), QStringLiteral("main"), id, QString(), false, false, 0));
      job.id = id;
      QVERIFY(window.jobStore()->addJobTransactional(job));
    }
    window.syncModelsFromJobStore();
    QCOMPARE(window.queueModel()->size(), 2);
    window.onMoveRequested(QStringLiteral("first"), 2);
    QCOMPARE(window.queueModel()->getItem(0).jobId, QStringLiteral("second"));
    disconnect(window.apiManager(), &APIManager::sessionCreationFailed, &window, nullptr);
    window.sendJobNow(QStringLiteral("second"));
    QCOMPARE(window.jobStore()->getJobById(QStringLiteral("second"))->attempts.size(), 1);
    QCOMPARE(window.queueModel()->size(), 1);
    QCOMPARE(window.queueModel()->getItem(0).jobId, QStringLiteral("first"));
  }

  void testWorkflowLaunchNewAttempt() {
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);

    JobStore *store = window.jobStore();
    QueueModel *queue = window.queueModel();
    QVERIFY(store != nullptr && queue != nullptr);

    JobData job;
    job.id = QStringLiteral("launch_new_workflow_job");
    job.canonicalRequest[QStringLiteral("prompt")] = QStringLiteral("Canonical default prompt");
    job.canonicalRequest[QStringLiteral("title")] = QStringLiteral("Canonical Title");
    store->addJobTransactional(job);

    SessionWindow sw(job.id, store, window.apiManager(), window.errorsModel(), true, &window);
    window.connectSessionWindow(&sw);

    QAction *newAttAct = sw.actionCollection()->action(QStringLiteral("launch_new_attempt"));
    QVERIFY(newAttAct != nullptr);
    newAttAct->trigger();

    // Enqueued with same Job ID and canonical defaults
    QCOMPARE(queue->size(), 1);
    QueueItem qItem = queue->getItem(0);
    QCOMPARE(qItem.jobId, QStringLiteral("launch_new_workflow_job"));
    QCOMPARE(qItem.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Canonical default prompt"));

    // Processing queue creates another attempt rather than a new Job
    disconnect(window.apiManager(), &APIManager::sessionCreationFailed, &window, nullptr);
    window.processQueueForTest();

    QCOMPARE(store->jobs().size(), 1);
    JobData *updatedJob = store->getJobById(QStringLiteral("launch_new_workflow_job"));
    QVERIFY(updatedJob != nullptr);
    QCOMPARE(updatedJob->attempts.size(), 1);
    QCOMPARE(updatedJob->attempts[0].requestSnapshot.value(QStringLiteral("prompt")).toString(),
             QStringLiteral("Canonical default prompt"));
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

void TestSessionWindow::testCreateRepoWorkflow() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  KConfigGroup queueConfig(KSharedConfig::openConfig(), QStringLiteral("Queue"));
  queueConfig.writeEntry(QStringLiteral("QueueMode"), QStringLiteral("asap"));
  queueConfig.sync();

  APIManager *api = window.apiManager();

  auto *mockNet = new MockCreateRepoAndSessionNetworkManager(api);
  api->injectNetworkAccessManagerForTesting(mockNet);
  api->setApiKey(QStringLiteral("test-key"));
  api->setGithubToken(QStringLiteral("gh-token"));

  QSignalSpy repoCreatedSpy(api, &APIManager::githubRepoCreated);
  QSignalSpy repoFailedSpy(api, &APIManager::githubRepoCreationFailed);
  QSignalSpy sessionCreatedSpy(api, &APIManager::sessionCreated);

  QueueModel *qm = window.queueModel();
  JobStore *js = window.jobStore();

  QCOMPARE(qm->size(), 0);

  // Invoke private onCreateRepoAndSession directly using friendship
  window.onCreateRepoAndSession(QStringLiteral("test-org"), QStringLiteral("test-repo"), true,
                                QStringLiteral("Fix issue 404"), QStringLiteral("AUTO_CREATE_PR"), true, false);

  QCOMPARE(qm->size(), 1);
  QueueItem initialItem = qm->getItem(0);
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_action")).toString(),
           QStringLiteral("create_github_repo"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Fix issue 404"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("automationMode")).toString(),
           QStringLiteral("AUTO_CREATE_PR"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_github_owner")).toString(),
           QStringLiteral("test-org"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_github_repository")).toString(),
           QStringLiteral("test-repo"));

  mockNet->interceptCreateRepo = true;
  mockNet->repoResponse = "{\"name\":\"test-repo\", \"full_name\":\"test-org/test-repo\"}";
  mockNet->interceptListSources = true;
  mockNet->listSourcesResponse = "{\"sources\":[]}";

  QSignalSpy initialRefreshSpy(api, &APIManager::sourcesRefreshFinished);
  window.processQueueForTest();

  QTRY_COMPARE(repoCreatedSpy.count(), 1);
  QTRY_COMPARE(initialRefreshSpy.count(), 1);
  QCOMPARE(repoFailedSpy.count(), 0);

  // Since wait for repo handles sync, the item is removed then immediately added to the top
  // Make sure it remains exactly one item
  QCOMPARE(qm->size(), 1);

  // Ensure it's waiting for source resolution and queue hasn't been consumed
  QSignalSpy waitingRefreshSpy(api, &APIManager::sourcesRefreshFinished);
  window.processQueueForTest(); // Try again while still waiting
  QTRY_COMPARE(waitingRefreshSpy.count(), 1);

  QCOMPARE(sessionCreatedSpy.count(), 0);
  QCOMPARE(qm->size(), 1); // Still in queue, waiting for source resolution

  QueueItem updatedItem = qm->getItem(0);
  JobData *job = js->getJobById(updatedItem.jobId);
  QVERIFY(job != nullptr);
  QVERIFY(!job->canonicalRequest.contains(QStringLiteral("_kjules_action")));
  QCOMPARE(job->canonicalRequest.value(QStringLiteral("prompt")).toString(), QStringLiteral("Fix issue 404"));

  // Verify that the repo creation attempt was marked as COMPLETED but NOT as a successful Jules execution
  QCOMPARE(job->attempts.size(), 1);
  QCOMPARE(job->attempts[0].dispatchState, QStringLiteral("COMPLETED"));
  QVERIFY(job->attempts[0].julesState.isEmpty());
  QVERIFY(JobPolicy::isAttemptTerminal(job->attempts[0])); // It no longer consumes concurrency
  QVERIFY(!JobPolicy::consumesConcurrency(job->attempts[0]));
  QVERIFY(!JobPolicy::isAttemptSuccessful(job->attempts[0])); // Must NOT be treated as a successful Jules execution
  QVERIFY(!JobPolicy::isSuccessfullyComplete(*job));          // Job is not successfully complete
  QCOMPARE(JobPolicy::aggregateState(*job), JobPolicy::JobAggregateState::Pending); // Job remains Pending in queue

  // Emulate source discovery: new repository now appears in Jules sources
  QJsonArray sourcesResponse;
  QJsonObject newSource;
  newSource[QStringLiteral("id")] = QStringLiteral("sources/github/test-org/test-repo");
  newSource[QStringLiteral("name")] = QStringLiteral("sources/github/test-org/test-repo");
  QJsonObject githubRepo;
  githubRepo[QStringLiteral("owner")] = QStringLiteral("test-org");
  githubRepo[QStringLiteral("repo")] = QStringLiteral("test-repo");
  newSource[QStringLiteral("githubRepo")] = githubRepo;
  sourcesResponse.append(newSource);

  QJsonObject sourcesDoc;
  sourcesDoc[QStringLiteral("sources")] = sourcesResponse;
  mockNet->listSourcesResponse = QJsonDocument(sourcesDoc).toJson();

  QSignalSpy sourcesRefreshSpy(api, &APIManager::sourcesRefreshFinished);
  window.refreshSourcesImpl(true);
  QTRY_COMPARE(sourcesRefreshSpy.count(), 1);

  QueueItem resolvedItem = qm->getItem(0);
  QCOMPARE(resolvedItem.requestData.value(QStringLiteral("source")).toString(),
           QStringLiteral("sources/github/test-org/test-repo"));
  QVERIFY(!resolvedItem.requestData.contains(QStringLiteral("_kjules_github_owner")));
  QVERIFY(!resolvedItem.requestData.contains(QStringLiteral("_kjules_github_repository")));

  // Verify Job persistence of resolved source
  JobData *resolvedJob = js->getJobById(resolvedItem.jobId);
  QCOMPARE(resolvedJob->source, QStringLiteral("sources/github/test-org/test-repo"));
  QCOMPARE(resolvedJob->canonicalRequest.value(QStringLiteral("source")).toString(),
           QStringLiteral("sources/github/test-org/test-repo"));
  QVERIFY(!resolvedJob->canonicalRequest.contains(QStringLiteral("_kjules_github_owner")));
  QVERIFY(!resolvedJob->canonicalRequest.contains(QStringLiteral("_kjules_github_repository")));

  // Now process Queue should dispatch it
  mockNet->interceptCreateSession = true;
  mockNet->createSessionResponse = "{\"id\":\"sess-123\", \"state\":\"RUNNING\"}";

  window.processQueueForTest();

  QTRY_COMPARE(sessionCreatedSpy.count(), 1);
  QCOMPARE(qm->size(), 0);

  // Job should now have 2 attempts, and aggregateState should be Active (session is RUNNING)
  JobData *dispatchedJob = js->getJobById(resolvedItem.jobId);
  QVERIFY(dispatchedJob != nullptr);
  QCOMPARE(dispatchedJob->attempts.size(), 2);
  QCOMPARE(dispatchedJob->attempts[1].julesState, QStringLiteral("RUNNING"));
  QCOMPARE(JobPolicy::aggregateState(*dispatchedJob), JobPolicy::JobAggregateState::Active);

  // Verify exactly once (calling processQueue again does nothing)
  window.processQueueForTest();
  QCOMPARE(sessionCreatedSpy.count(), 1);

  // Verify request body sent over network
  QCOMPARE(mockNet->capturedSessionRequests.size(), 1);
  QJsonDocument sentDoc = QJsonDocument::fromJson(mockNet->capturedSessionRequests.first());
  QJsonObject sentReq = sentDoc.object();

  QCOMPARE(sentReq.value(QStringLiteral("prompt")).toString(), QStringLiteral("Fix issue 404"));
  QCOMPARE(sentReq.value(QStringLiteral("automationMode")).toString(), QStringLiteral("AUTO_CREATE_PR"));
  QCOMPARE(sentReq.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QJsonObject sourceCtx = sentReq.value(QStringLiteral("sourceContext")).toObject();
  QCOMPARE(sourceCtx.value(QStringLiteral("source")).toString(), QStringLiteral("sources/github/test-org/test-repo"));
}

void TestSessionWindow::testCreateRepoWorkflowRepoFailure() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  KConfigGroup queueConfig(KSharedConfig::openConfig(), QStringLiteral("Queue"));
  queueConfig.writeEntry(QStringLiteral("QueueMode"), QStringLiteral("asap"));
  queueConfig.sync();

  APIManager *api = window.apiManager();
  auto *mockNet = new MockCreateRepoAndSessionNetworkManager(api);
  api->injectNetworkAccessManagerForTesting(mockNet);
  api->setApiKey(QStringLiteral("test-key"));
  api->setGithubToken(QStringLiteral("gh-token"));

  QSignalSpy repoFailedSpy(api, &APIManager::githubRepoCreationFailed);
  QSignalSpy sessionCreatedSpy(api, &APIManager::sessionCreated);

  QueueModel *qm = window.queueModel();
  JobStore *js = window.jobStore();

  window.onCreateRepoAndSession(QStringLiteral("test-org"), QStringLiteral("fail-repo"), true,
                                QStringLiteral("Fail test prompt"), QStringLiteral("AUTO_CREATE_PR"), false, false);

  QCOMPARE(qm->size(), 1);

  mockNet->interceptCreateRepo = true;
  mockNet->repoStatusCode = 422;
  mockNet->repoResponse = "{\"message\":\"Repository already exists\"}";

  window.processQueueForTest();

  QTRY_COMPARE(repoFailedSpy.count(), 1);
  QCOMPARE(sessionCreatedSpy.count(), 0);

  // Queue item was removed during dispatch, and on failure it is NOT re-added to queue as pending source
  QCOMPARE(qm->size(), 0);

  // Job should record the attempt as terminal failure and concurrency is not consumed
  QList<JobData> jobs = js->jobs();
  QVERIFY(!jobs.isEmpty());
  JobData *failedJob = js->getJobById(jobs.first().id);
  QVERIFY(failedJob != nullptr);
  QCOMPARE(failedJob->attempts.size(), 1);
  const JobAttemptData &attempt = failedJob->attempts.first();
  QCOMPARE(attempt.dispatchState, QStringLiteral("FAILED"));
  QVERIFY(attempt.julesState.isEmpty());
  QVERIFY(JobPolicy::isAttemptTerminal(attempt));
  QVERIFY(JobPolicy::isAttemptFailed(attempt));
  QVERIFY(!JobPolicy::isAttemptSuccessful(attempt));
  QVERIFY(!JobPolicy::consumesConcurrency(attempt));
  QVERIFY(!JobPolicy::isSuccessfullyComplete(*failedJob));
  QCOMPARE(JobPolicy::aggregateState(*failedJob), JobPolicy::JobAggregateState::NeedsAttention);
  QVERIFY(!attempt.launchErrors.isEmpty());

  // Concurrency accounting remains 0
  int globalActive = 0;
  for (const JobData &j : js->jobs()) {
    for (const JobAttemptData &a : j.attempts) {
      if (JobPolicy::consumesConcurrency(a)) {
        globalActive++;
      }
    }
  }
  QCOMPARE(globalActive, 0);
}

void TestSessionWindow::testCreateRepoWorkflowSourceRefreshRetry() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  KConfigGroup queueConfig(KSharedConfig::openConfig(), QStringLiteral("Queue"));
  queueConfig.writeEntry(QStringLiteral("QueueMode"), QStringLiteral("asap"));
  queueConfig.writeEntry(QStringLiteral("TimerInterval"), 1);
  queueConfig.sync();

  APIManager *api = window.apiManager();
  auto *mockNet = new MockCreateRepoAndSessionNetworkManager(api);
  api->injectNetworkAccessManagerForTesting(mockNet);
  api->setApiKey(QStringLiteral("test-key"));
  api->setGithubToken(QStringLiteral("gh-token"));

  QSignalSpy repoCreatedSpy(api, &APIManager::githubRepoCreated);
  QSignalSpy sessionCreatedSpy(api, &APIManager::sessionCreated);

  QueueModel *qm = window.queueModel();
  JobStore *js = window.jobStore();

  // 1. Start create-repo-and-session Job
  window.onCreateRepoAndSession(QStringLiteral("test-org"), QStringLiteral("retry-repo"), true,
                                QStringLiteral("Retry test prompt"), QStringLiteral("AUTO_CREATE_PR"), true, false);

  QCOMPARE(qm->size(), 1);
  QueueItem initialItem = qm->getItem(0);
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_action")).toString(),
           QStringLiteral("create_github_repo"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Retry test prompt"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("automationMode")).toString(),
           QStringLiteral("AUTO_CREATE_PR"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_github_owner")).toString(),
           QStringLiteral("test-org"));
  QCOMPARE(initialItem.requestData.value(QStringLiteral("_kjules_github_repository")).toString(),
           QStringLiteral("retry-repo"));

  // 2. GitHub repo creation succeeds
  mockNet->interceptCreateRepo = true;
  mockNet->repoResponse = "{\"name\":\"retry-repo\", \"full_name\":\"test-org/retry-repo\"}";

  // 3 & 4. Initial Jules source discovery runs as background work and fails with 500 error
  mockNet->interceptListSources = true;
  mockNet->listSourcesStatusCode = 500;
  mockNet->listSourcesResponse = "{\"error\":\"internal error\"}";

  QSignalSpy initialRefreshSpy(api, &APIManager::sourcesRefreshFinished);
  window.processQueueForTest();
  QTRY_COMPARE(repoCreatedSpy.count(), 1);
  QTRY_COMPARE(initialRefreshSpy.count(), 1);

  // 5. Incomplete/failed refresh must not discard pending work
  QCOMPARE(qm->size(), 1);
  QCOMPARE(sessionCreatedSpy.count(), 0);
  QVERIFY(window.m_isWaitingForCreatedRepoSource);

  QueueItem pendingItem = qm->getItem(0);
  QString jobId = pendingItem.jobId;
  QVERIFY(!jobId.isEmpty());
  JobData *job = js->getJobById(jobId);
  QVERIFY(job != nullptr);
  QCOMPARE(JobPolicy::aggregateState(*job), JobPolicy::JobAggregateState::Pending);

  QCOMPARE(pendingItem.jobId, jobId);
  QVERIFY(!pendingItem.requestData.contains(QStringLiteral("_kjules_action")));
  QCOMPARE(pendingItem.requestData.value(QStringLiteral("_kjules_github_owner")).toString(),
           QStringLiteral("test-org"));
  QCOMPARE(pendingItem.requestData.value(QStringLiteral("_kjules_github_repository")).toString(),
           QStringLiteral("retry-repo"));
  QCOMPARE(pendingItem.requestData.value(QStringLiteral("prompt")).toString(), QStringLiteral("Retry test prompt"));

  // Assert: Job remains Pending while waiting; provisioning attempt is terminal and non-concurrency-consuming
  job = js->getJobById(jobId);
  QVERIFY(job != nullptr);
  QCOMPARE(JobPolicy::aggregateState(*job), JobPolicy::JobAggregateState::Pending);
  QCOMPARE(job->attempts.size(), 1);
  const JobAttemptData &provisionAttempt = job->attempts[0];
  QVERIFY(JobPolicy::isRepoProvisioningAttempt(provisionAttempt));
  QCOMPARE(provisionAttempt.dispatchState, QStringLiteral("COMPLETED"));
  QVERIFY(JobPolicy::isAttemptTerminal(provisionAttempt));
  QVERIFY(!JobPolicy::consumesConcurrency(provisionAttempt));
  QVERIFY(!JobPolicy::isAttemptSuccessful(provisionAttempt));
  QVERIFY(!JobPolicy::isEligibleWinner(*job, provisionAttempt.id));

  // 6. Production retry mechanism schedules the next queue attempt
  QVERIFY(window.m_queueScheduler.nextQueueProcessAt().isValid());
  QVERIFY(window.m_queueScheduler.nextQueueProcessAt() > QDateTime::currentDateTimeUtc());

  // A minute timer tick before the scheduled interval must NOT trigger queue work prematurely
  window.onMasterMinuteTimer();
  QCOMPARE(qm->size(), 1);
  QCOMPARE(sessionCreatedSpy.count(), 0);
  QVERIFY(window.m_isWaitingForCreatedRepoSource);
  QVERIFY(!window.m_isRefreshingSources);

  // 7 & 8. Configure responses for when the scheduled retry occurs: source appears and session succeeds
  mockNet->listSourcesStatusCode = 200;
  QJsonArray sourcesArr;
  QJsonObject newSource;
  newSource[QStringLiteral("id")] = QStringLiteral("sources/github/test-org/retry-repo");
  newSource[QStringLiteral("name")] = QStringLiteral("sources/github/test-org/retry-repo");
  QJsonObject ghRepo;
  ghRepo[QStringLiteral("owner")] = QStringLiteral("test-org");
  ghRepo[QStringLiteral("repo")] = QStringLiteral("retry-repo");
  newSource[QStringLiteral("githubRepo")] = ghRepo;
  sourcesArr.append(newSource);
  QJsonObject docObj;
  docObj[QStringLiteral("sources")] = sourcesArr;
  mockNet->listSourcesResponse = QJsonDocument(docObj).toJson();

  mockNet->interceptCreateSession = true;
  mockNet->createSessionResponse = "{\"id\":\"sess-retry\", \"state\":\"RUNNING\"}";

  // Advance time to the scheduled queue attempt
  window.m_queueScheduler.setNextProcessAt(QDateTime::currentDateTimeUtc().addSecs(-1));
  QVERIFY(window.m_queueScheduler.isDue(QDateTime::currentDateTimeUtc()));

  QSignalSpy retryRefreshSpy(api, &APIManager::sourcesRefreshFinished);

  // Drive the retry via the production scheduler/queue mechanism (without calling refreshSourcesImpl directly)
  window.onMasterMinuteTimer();

  // Assert: Autonomous retry is classified as background and foreground UI is not activated
  QVERIFY(window.m_isRefreshingSources);
  QVERIFY(window.m_isBackgroundSourcesRefresh);
  QVERIFY(window.m_sourceProgressBar->isHidden());
  QVERIFY(window.m_cancelRefreshBtn->isHidden());
  QCOMPARE(window.m_refreshSourcesAction->text(), QStringLiteral("Refresh Sources"));

  // Wait for background source discovery to complete
  QTRY_COMPARE(retryRefreshSpy.count(), 1);

  // 9 & 10. Source is resolved and exactly one Jules session is dispatched
  QVERIFY(!window.m_isWaitingForCreatedRepoSource);
  QTRY_COMPARE(sessionCreatedSpy.count(), 1);
  QCOMPARE(qm->size(), 0);

  // Assert: Job transitions to Active, source resolved, and real Jules session consumes concurrency
  job = js->getJobById(jobId);
  QVERIFY(job != nullptr);
  QCOMPARE(job->source, QStringLiteral("sources/github/test-org/retry-repo"));
  QCOMPARE(job->canonicalRequest.value(QStringLiteral("source")).toString(),
           QStringLiteral("sources/github/test-org/retry-repo"));
  QVERIFY(!job->canonicalRequest.contains(QStringLiteral("_kjules_github_owner")));
  QVERIFY(!job->canonicalRequest.contains(QStringLiteral("_kjules_github_repository")));
  QCOMPARE(job->attempts.size(), 2);

  const JobAttemptData &julesAttempt = job->attempts[1];
  QVERIFY(!JobPolicy::isRepoProvisioningAttempt(julesAttempt));
  QCOMPARE(julesAttempt.julesSessionId, QStringLiteral("sess-retry"));
  QCOMPARE(julesAttempt.julesState, QStringLiteral("RUNNING"));
  QVERIFY(!JobPolicy::isAttemptTerminal(julesAttempt));
  QVERIFY(JobPolicy::consumesConcurrency(julesAttempt));
  QCOMPARE(JobPolicy::aggregateState(*job), JobPolicy::JobAggregateState::Active);

  // Verify captured network request for session creation
  QCOMPARE(mockNet->capturedSessionRequests.size(), 1);
  QJsonDocument sentDoc = QJsonDocument::fromJson(mockNet->capturedSessionRequests.first());
  QJsonObject sentReq = sentDoc.object();
  QCOMPARE(sentReq.value(QStringLiteral("prompt")).toString(), QStringLiteral("Retry test prompt"));
  QCOMPARE(sentReq.value(QStringLiteral("automationMode")).toString(), QStringLiteral("AUTO_CREATE_PR"));
  QCOMPARE(sentReq.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QJsonObject sourceCtx = sentReq.value(QStringLiteral("sourceContext")).toObject();
  QCOMPARE(sourceCtx.value(QStringLiteral("source")).toString(), QStringLiteral("sources/github/test-org/retry-repo"));

  // 11. Duplicate queue/scheduler ticks or refresh completion events do not create another session
  window.onMasterMinuteTimer();
  window.processQueueForTest();
  window.onSourcesRefreshFinished(true);

  QCOMPARE(sessionCreatedSpy.count(), 1);
  QCOMPARE(mockNet->createSessionCount, 1);
  QCOMPARE(qm->size(), 0);
}

void TestSessionWindow::testProcessQueueIndependenceFromFollowingReloads() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  KConfigGroup queueConfig(KSharedConfig::openConfig(), QStringLiteral("Queue"));
  queueConfig.writeEntry(QStringLiteral("QueueMode"), QStringLiteral("asap"));
  queueConfig.writeEntry(QStringLiteral("TimerInterval"), 1); // Every minute
  queueConfig.sync();

  KConfigGroup sessionConfig(KSharedConfig::openConfig(), QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry(QStringLiteral("FollowingAutoRefreshInterval"), 1); // Force immediate expiration
  sessionConfig.sync();

  APIManager *api = window.apiManager();
  auto *mockNet = new MockCreateRepoAndSessionNetworkManager(api);
  api->injectNetworkAccessManagerForTesting(mockNet);
  api->setApiKey(QStringLiteral("test-key"));
  api->setGithubToken(QStringLiteral("gh-token"));

  QSignalSpy sessionCreatedSpy(api, &APIManager::sessionCreated);

  // Setup a stale following session
  QJsonObject sessObj;
  sessObj[QStringLiteral("id")] = QStringLiteral("stale-sess-1");
  sessObj[QStringLiteral("state")] = QStringLiteral("RUNNING"); // Eligible for refresh
  sessObj[QStringLiteral("lastRefreshed")] = QDateTime::currentDateTimeUtc().addSecs(-60).toString(Qt::ISODate);
  window.sessionModel()->addSession(sessObj);

  // Set up queue with an item ready to process
  QueueModel *qm = window.queueModel();
  QueueItem item;
  item.jobId = QStringLiteral("queued-job-1");
  item.requestData = SessionRequestBuilder::buildSessionRequest(
      QStringLiteral("sources/github/test/repo"), QStringLiteral("test-branch"),
      QStringLiteral("Prompt"), QStringLiteral("AUTO_CREATE_PR"));
  qm->enqueueItem(item);

  // Ensure queue is due
  window.m_queueScheduler.setNextProcessAt(QDateTime::currentDateTimeUtc().addSecs(-60));

  // Verify before processing
  QCOMPARE(qm->size(), 1);
  QCOMPARE(sessionCreatedSpy.count(), 0);

  // Invoke the master minute timer which should trigger auto refresh AND process queue
  window.onMasterMinuteTimer();

  // Test that processQueue() was able to start immediately (queue size drops as it processes and session is created)
  // because it's no longer gated by a m_isWaitingForRefreshBeforeQueue state or the completion of the auto refresh
  QCOMPARE(qm->size(), 0);
  QCOMPARE(sessionCreatedSpy.count(), 1);
  QCOMPARE(mockNet->createSessionCount, 1);
}

void TestSessionWindow::testCreateRepoWorkflowBackgroundClassification() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  APIManager *api = window.apiManager();
  auto *mockNet = new MockCreateRepoAndSessionNetworkManager(api);
  api->injectNetworkAccessManagerForTesting(mockNet);
  api->setApiKey(QStringLiteral("test-key"));

  mockNet->interceptListSources = true;
  mockNet->listSourcesResponse = "{\"sources\":[]}";

  // 1. Background refresh: must not show foreground UI (progress bar, cancel button)
  // and action text must remain "Refresh Sources"
  window.refreshSourcesImpl(true);
  QVERIFY(window.m_isBackgroundSourcesRefresh);
  QVERIFY(window.m_sourceProgressBar->isHidden());
  QVERIFY(window.m_cancelRefreshBtn->isHidden());
  QCOMPARE(window.m_refreshSourcesAction->text(), QStringLiteral("Refresh Sources"));

  // While background refresh is running, a second background refresh must NOT cancel it
  window.refreshSourcesImpl(true);
  QVERIFY(window.m_isRefreshingSources);

  // Wait for finish
  QSignalSpy refreshSpy(api, &APIManager::sourcesRefreshFinished);
  QTRY_COMPARE(refreshSpy.count(), 1);
  QVERIFY(!window.m_isRefreshingSources);
  QVERIFY(!window.m_isBackgroundSourcesRefresh);

  // 2. Foreground refresh (manual user action): must show progress bar, cancel button,
  // and action text "Cancel Refresh"
  window.refreshSources(); // refreshSourcesImpl(false)
  QVERIFY(!window.m_isBackgroundSourcesRefresh);
  QVERIFY(window.m_isRefreshingSources);
  QVERIFY(!window.m_sourceProgressBar->isHidden());
  QVERIFY(!window.m_cancelRefreshBtn->isHidden());
  QCOMPARE(window.m_refreshSourcesAction->text(), QStringLiteral("Cancel Refresh"));

  // Calling refreshSources() again while active acts as user clicking "Cancel Refresh"
  window.refreshSources();
  QVERIFY(!window.m_isRefreshingSources);
  QVERIFY(window.m_sourceProgressBar->isHidden());
  QVERIFY(window.m_cancelRefreshBtn->isHidden());
  QCOMPARE(window.m_refreshSourcesAction->text(), QStringLiteral("Refresh Sources"));
}
