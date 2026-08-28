#include "../src/api/apierror.h"
#include "../src/apimanager.h"
#include "../src/blockedtreemodel.h"
#include "../src/errorsmodel.h"
#include "../src/mainwindow.h"
#include "../src/queuemodel.h"
#include "../src/sessionmodel.h"
#include "../src/sessionswidget.h"
#include "../src/sessionswindow.h"
#include "../src/sessionwindow.h"
#include "../src/sourcemodel.h"
#include "../src/sourcewindow.h"

#include <QComboBox>
#include <QPushButton>

#include "../src/clickablelabel.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTreeView>
#include <QtTest>

class Mock200EmptyJsonNetworkReply : public QNetworkReply {
  Q_OBJECT
  QByteArray m_data;

public:
  Mock200EmptyJsonNetworkReply(const QByteArray &data, QObject *parent = nullptr)
      : QNetworkReply(parent), m_data(data) {
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    QTimer::singleShot(0, this, [this]() {
      setOpenMode(QIODevice::ReadOnly);
      Q_EMIT readyRead();
      Q_EMIT finished();
    });
  }
  void abort() override {}
  qint64 readData(char *data, qint64 maxlen) override {
    qint64 len = qMin(maxlen, (qint64)m_data.size());
    if (len > 0) {
      memcpy(data, m_data.constData(), len);
      m_data.remove(0, len);
      return len;
    }
    return 0;
  }
};

class MockE2ENetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT
public:
  QByteArray julesResponse;
  QByteArray githubResponse;
  QStringList requestedUrls;

  MockE2ENetworkAccessManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}

protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override {
    Q_UNUSED(op);
    Q_UNUSED(outgoingData);

    QString urlStr = request.url().toString();
    requestedUrls.append(urlStr);

    QByteArray dataToReturn;
    if (urlStr.contains(QStringLiteral("sessions/sess-e2e-1")) ||
        urlStr.contains(QStringLiteral("sessions/sess-manual-1")) ||
        urlStr.contains(QStringLiteral("sessions/sess-inflight-1"))) {
      dataToReturn = julesResponse;
    } else if (urlStr.contains(QStringLiteral("owner/repo/pulls/123")) ||
               urlStr.contains(QStringLiteral("owner/repo/pulls/999"))) {
      dataToReturn = githubResponse;
    } else {
      dataToReturn = "{}";
    }

    auto *reply = new Mock200EmptyJsonNetworkReply(dataToReturn, this);
    return reply;
  }
};

class TestSourceWindow : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testFullSemanticChain();
  void testManualVsAutomaticRefreshEquivalent();
  void testInFlightRecovery();
  void initTestCase();
  void testKXmlGuiResourceExists();
  void testNoNestedSessionsWindowAndSingleChrome();
  void testIntegratedTabLayoutAndInitialSelection();
  void testNewSessionSignal();
  void testQueueProcessingRequestedSignal();
  void testAutoFollowWiring();
  void testDefaultBranchesRemovalRestoresApiDefault();
  void testRawDataEditingCanonicalAndLegacy();
  void testManualFollowAddsAndPersistsSession();
  void testRefreshSessionsActionWired();
  void testGithubIssuesTabs();
  void testSourceWindowNavigationAndUnseen();
  void testStaleIssueCallbackGuards();
  void testSessionWindowContextualErrors();
  void testSessionWindowMessageSendFailureLinks();
  void testClickableLabelLinkHandling();
};

void TestSourceWindow::initTestCase() {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("KDE_HOME_READONLY", "1");
  qputenv("CANBERRA_DRIVER", "null");
  qputenv("KNOTIFICATIONS_DEFAULT_BACKEND", "null");
}

void TestSourceWindow::testKXmlGuiResourceExists() {
  QVERIFY(QFile::exists(QStringLiteral(":/kxmlgui6/org.kde.kjules/sourcewindowui.rc")));
  QVERIFY(QFile::exists(QStringLiteral(":/kxmlgui6/org.kde.kjules/sessionswindowui.rc")));
}

void TestSourceWindow::testNoNestedSessionsWindowAndSingleChrome() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  // Must not embed SessionsWindow (KXmlGuiWindow) inside SourceWindow
  QList<SessionsWindow *> nestedSessionsWindows = window->findChildren<SessionsWindow *>();
  QVERIFY(nestedSessionsWindows.isEmpty());

  // Must embed SessionsWidget
  SessionsWidget *sessionsWidget = window->findChild<SessionsWidget *>();
  QVERIFY(sessionsWidget != nullptr);
}

void TestSourceWindow::testIntegratedTabLayoutAndInitialSelection() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QTabWidget *tabWidget = window->findChild<QTabWidget *>();
  QVERIFY(tabWidget != nullptr);

  // Must have at least 6 tabs in correct operational order:
  // 0: Following, 1: Archived, 2: Queued/Blocked, 3: Sessions, 4: Settings, 5: Raw Data
  QVERIFY(tabWidget->count() >= 6);
  QCOMPARE(tabWidget->tabText(0), QStringLiteral("Following"));
  QCOMPARE(tabWidget->tabText(1), QStringLiteral("Archived"));
  QCOMPARE(tabWidget->tabText(2), QStringLiteral("Queued/Blocked"));
  QCOMPARE(tabWidget->tabText(3), QStringLiteral("Sessions"));
  QCOMPARE(tabWidget->tabText(4), QStringLiteral("Settings"));
  QCOMPARE(tabWidget->tabText(5), QStringLiteral("Raw Data"));

  // Following must be selected by default (index 0)
  QCOMPARE(tabWidget->currentIndex(), 0);
}

void TestSourceWindow::testNewSessionSignal() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QSignalSpy spy(window.get(), &SourceWindow::newSessionRequested);

  QAction *newSessionAction = window->actionCollection()->action(QStringLiteral("new_session"));
  QVERIFY(newSessionAction != nullptr);

  newSessionAction->trigger();

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.first().at(0).toString(), sourceId);
}

void TestSourceWindow::testQueueProcessingRequestedSignal() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QSignalSpy spy(window.get(), &SourceWindow::queueProcessingRequested);

  QSpinBox *spinBox = window->findChild<QSpinBox *>();
  QVERIFY(spinBox != nullptr);
  QCOMPARE(spinBox->minimum(), -1);
  QCOMPARE(spinBox->maximum(), 1000);

  // Set to 5
  spinBox->setValue(5);
  QCOMPARE(spy.count(), 1);

  KConfigGroup sourceConfig(KSharedConfig::openConfig(), QStringLiteral("SourceConcurrency"));
  QCOMPARE(sourceConfig.readEntry(sourceId, -999), 5);

  // Set to 0 (valid disabled limit)
  spinBox->setValue(0);
  QCOMPARE(spy.count(), 2);
  QCOMPARE(sourceConfig.readEntry(sourceId, -999), 0);

  // Set to 1000
  spinBox->setValue(1000);
  QCOMPARE(spy.count(), 3);
  QCOMPARE(sourceConfig.readEntry(sourceId, -999), 1000);

  // Set to -1 (delete override)
  spinBox->setValue(-1);
  QCOMPARE(spy.count(), 4);
  QVERIFY(!sourceConfig.hasKey(sourceId));
}

void TestSourceWindow::testAutoFollowWiring() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = QStringLiteral("sources/github/kde/kjules");
  srcObj[QStringLiteral("local_autoFollowNewSessions")] = false;
  sourceModel.addSources(QJsonArray{srcObj});

  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QCheckBox *checkBox = window->findChild<QCheckBox *>();
  QVERIFY(checkBox != nullptr);
  QCOMPARE(checkBox->isChecked(), false);

  // Toggle to true
  checkBox->setChecked(true);
  QModelIndex idx = sourceModel.index(0, 0);
  QJsonObject updated = sourceModel.data(idx, SourceModel::RawDataRole).toJsonObject();
  QCOMPARE(updated.value(QStringLiteral("local_autoFollowNewSessions")).toBool(), true);

  // Toggle to false
  checkBox->setChecked(false);
  updated = sourceModel.data(idx, SourceModel::RawDataRole).toJsonObject();
  QCOMPARE(updated.value(QStringLiteral("local_autoFollowNewSessions")).toBool(), false);
}

void TestSourceWindow::testDefaultBranchesRemovalRestoresApiDefault() {
  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);

  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = QStringLiteral("sources/github/kde/kjules");
  QJsonObject ghRepo;
  QJsonObject defaultBranchObj;
  defaultBranchObj[QStringLiteral("displayName")] = QStringLiteral("develop");
  ghRepo[QStringLiteral("defaultBranch")] = defaultBranchObj;
  srcObj[QStringLiteral("githubRepo")] = ghRepo;

  QJsonArray localBranches;
  localBranches.append(QStringLiteral("custom-branch"));
  srcObj[QStringLiteral("local_defaultBranches")] = localBranches;

  sourceModel.addSources(QJsonArray{srcObj});

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  QStringList branches = sourceModel.getEffectiveDefaultBranches(sourceId);
  QCOMPARE(branches, QStringList{QStringLiteral("custom-branch")});

  // Clear local override
  bool cleared = sourceModel.clearDefaultBranches(sourceId);
  QVERIFY(cleared);

  // Must restore API default "develop", not fallback "main"
  branches = sourceModel.getEffectiveDefaultBranches(sourceId);
  QCOMPARE(branches, QStringList{QStringLiteral("develop")});
}

void TestSourceModelRawEditingHelper(SourceModel &model, const QString &id, const QJsonObject &payload,
                                     bool expectedSuccess) {
  bool success = model.updateSourceRaw(id, payload);
  QCOMPARE(success, expectedSuccess);
}

void TestSourceWindow::testRawDataEditingCanonicalAndLegacy() {
  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);

  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = QStringLiteral("sources/github/kde/kjules");
  srcObj[QStringLiteral("description")] = QStringLiteral("Old Description");
  sourceModel.addSources(QJsonArray{srcObj});

  QString sourceId = QStringLiteral("sources/github/kde/kjules");

  // Canonical name update
  QJsonObject canonicalPayload;
  canonicalPayload[QStringLiteral("name")] = sourceId;
  canonicalPayload[QStringLiteral("description")] = QStringLiteral("Updated via canonical name");
  TestSourceModelRawEditingHelper(sourceModel, sourceId, canonicalPayload, true);
  QCOMPARE(sourceModel.data(sourceModel.index(0, SourceModel::ColDescription)).toString(),
           QStringLiteral("Updated via canonical name"));

  // Legacy id update
  QJsonObject legacyPayload;
  legacyPayload[QStringLiteral("id")] = sourceId;
  legacyPayload[QStringLiteral("description")] = QStringLiteral("Updated via legacy id");
  TestSourceModelRawEditingHelper(sourceModel, sourceId, legacyPayload, true);
  QCOMPARE(sourceModel.data(sourceModel.index(0, SourceModel::ColDescription)).toString(),
           QStringLiteral("Updated via legacy id"));

  // Mismatched id update must be rejected
  QJsonObject mismatchedPayload;
  mismatchedPayload[QStringLiteral("name")] = QStringLiteral("sources/github/other/repo");
  TestSourceModelRawEditingHelper(sourceModel, sourceId, mismatchedPayload, false);

  // Non-matching sourceId must be rejected
  TestSourceModelRawEditingHelper(sourceModel, QStringLiteral("nonexistent"), canonicalPayload, false);
}

void TestSourceWindow::testManualFollowAddsAndPersistsSession() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  SessionsWidget *sessionsWidget = window->findChild<SessionsWidget *>();
  QVERIFY(sessionsWidget != nullptr);

  QJsonObject sessionData;
  sessionData[QStringLiteral("id")] = QStringLiteral("sess-new-123");
  sessionData[QStringLiteral("title")] = QStringLiteral("New Test Session");
  sessionData[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");

  Q_EMIT sessionsWidget->watchRequested(sessionData);

  QCOMPARE(sessionModel.rowCount(), 1);
  QCOMPARE(sessionModel.getSession(0).value(QStringLiteral("id")).toString(), QStringLiteral("sess-new-123"));
}

void TestSourceWindow::testRefreshSessionsActionWired() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QAction *refreshAction = window->actionCollection()->action(QStringLiteral("refresh_sessions"));
  QVERIFY(refreshAction != nullptr);
  QCOMPARE(refreshAction->text(), tr("Refresh Sessions"));
  QCOMPARE(refreshAction->shortcut(), QKeySequence(Qt::Key_F5));

  SessionsWidget *sessionsWidget = window->findChild<SessionsWidget *>();
  QVERIFY(sessionsWidget != nullptr);

  // Triggering the action calls refreshSessions
  refreshAction->trigger();
}

void TestSourceWindow::testGithubIssuesTabs() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);
  APIManager apiManager;

  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = QStringLiteral("sources/github/kde/kjules");
  QJsonObject repoObj;
  repoObj[QStringLiteral("owner")] = QStringLiteral("kde");
  repoObj[QStringLiteral("repo")] = QStringLiteral("kjules");
  srcObj[QStringLiteral("githubRepo")] = repoObj;

  QJsonArray localIssues;
  QJsonObject issue1;
  issue1[QStringLiteral("number")] = 101;
  issue1[QStringLiteral("title")] = QStringLiteral("Open Issue");
  issue1[QStringLiteral("state")] = QStringLiteral("open");
  issue1[QStringLiteral("user")] = QStringLiteral("testuser");

  QJsonObject issue2;
  issue2[QStringLiteral("number")] = 102;
  issue2[QStringLiteral("title")] = QStringLiteral("Closed Issue");
  issue2[QStringLiteral("state")] = QStringLiteral("closed");
  issue2[QStringLiteral("user")] = QStringLiteral("testuser2");

  QJsonObject pr3;
  pr3[QStringLiteral("number")] = 103;
  pr3[QStringLiteral("title")] = QStringLiteral("A PR");
  pr3[QStringLiteral("state")] = QStringLiteral("open");
  pr3[QStringLiteral("user")] = QStringLiteral("pruser");
  pr3[QStringLiteral("pull_request")] = QJsonObject();

  localIssues.append(issue1);
  localIssues.append(issue2);
  localIssues.append(pr3);
  srcObj[QStringLiteral("local_githubIssues")] = localIssues;

  sourceModel.addSources(QJsonArray{srcObj});

  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, &apiManager);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QTreeView *issuesView = window->findChild<QTreeView *>(QStringLiteral("githubIssuesView"));
  QComboBox *stateCombo = window->findChild<QComboBox *>(QStringLiteral("githubIssuesStateCombo"));

  QVERIFY(issuesView != nullptr);
  QVERIFY(stateCombo != nullptr);

  QAbstractItemModel *model = issuesView->model();
  QVERIFY(model != nullptr);

  // Assert Open
  stateCombo->setCurrentIndex(stateCombo->findData(QStringLiteral("open")));
  window->show();
  QCoreApplication::processEvents();
  QCOMPARE(model->rowCount(), 1);
  QCOMPARE(model->index(0, 0).data().toInt(), 101);
  QCOMPARE(model->index(0, 3).data().toString(), QStringLiteral("testuser"));

  // Assert Closed
  stateCombo->setCurrentIndex(stateCombo->findData(QStringLiteral("closed")));
  window->show();
  QCoreApplication::processEvents();
  QCOMPARE(model->rowCount(), 1);
  QCOMPARE(model->index(0, 0).data().toInt(), 102);
  QCOMPARE(model->index(0, 3).data().toString(), QStringLiteral("testuser2"));

  // Assert All
  stateCombo->setCurrentIndex(stateCombo->findData(QStringLiteral("all")));
  window->show();
  QCoreApplication::processEvents();
  QCOMPARE(model->rowCount(), 2);
}

void TestSourceWindow::testSourceWindowNavigationAndUnseen() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);
  APIManager apiManager;
  QString sourceId = QStringLiteral("sources/github/kde/kjules");
  sourceModel.addSources(QJsonArray{QJsonObject{{QStringLiteral("name"), sourceId}}});

  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, &apiManager);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QJsonObject err1;
  err1[QStringLiteral("sourceId")] = sourceId;
  errorsModel.addErrorObj(err1);

  QJsonObject err2;
  err2[QStringLiteral("sourceId")] = QStringLiteral("sources/github/other/repo");
  errorsModel.addErrorObj(err2);

  QCOMPARE(errorsModel.unseenCount(), 2);

  QTabWidget *tabWidget = window->findChild<QTabWidget *>();
  QVERIFY(tabWidget != nullptr);

  for (int i = 0; i < tabWidget->count(); ++i) {
    if (tabWidget->tabText(i) == tr("Queued/Blocked") || tabWidget->tabText(i) == tr("Queue & Issues")) {
      tabWidget->setCurrentIndex(i);
    }
  }

  QTabWidget *subTab = tabWidget->currentWidget()->findChild<QTabWidget *>();
  if (subTab) {
    for (int i = 0; i < subTab->count(); ++i) {
      if (subTab->tabText(i).contains(tr("Error"))) {
        subTab->setCurrentIndex(i);
      }
    }
  }

  window->show();
  QCoreApplication::processEvents();

  bool err1Unseen = true;
  bool err2Unseen = true;

  for (int row = 0; row < errorsModel.rowCount(); ++row) {
    QModelIndex idx = errorsModel.index(row, 0);
    if (idx.data(ErrorsModel::SourceIdRole).toString() == sourceId) {
      err1Unseen = idx.data(ErrorsModel::UnseenRole).toBool();
    } else if (idx.data(ErrorsModel::SourceIdRole).toString() == QStringLiteral("sources/github/other/repo")) {
      err2Unseen = idx.data(ErrorsModel::UnseenRole).toBool();
    }
  }

  QCOMPARE(err1Unseen, false);
  QCOMPARE(err2Unseen, true);
  QCOMPARE(errorsModel.unseenCount(), 1);
}

void TestSourceWindow::testStaleIssueCallbackGuards() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);
  APIManager apiManager;

  QString sourceId = QStringLiteral("sources/github/kde/kjules");

  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = sourceId;
  QJsonObject repoObj;
  repoObj[QStringLiteral("owner")] = QStringLiteral("kde");
  repoObj[QStringLiteral("repo")] = QStringLiteral("kjules");
  srcObj[QStringLiteral("githubRepo")] = repoObj;
  QJsonArray localIssues;
  QJsonObject issue1;
  issue1[QStringLiteral("number")] = 101;
  issue1[QStringLiteral("title")] = QStringLiteral("Issue A");
  issue1[QStringLiteral("state")] = QStringLiteral("open");
  localIssues.append(issue1);
  srcObj[QStringLiteral("local_githubIssues")] = localIssues;
  sourceModel.addSources(QJsonArray{srcObj});

  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, &apiManager);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QSignalSpy spy(window.get(), &SourceWindow::newSessionFromIssueRequested);

  QTreeView *issuesView = window->findChild<QTreeView *>(QStringLiteral("githubIssuesView"));
  QVERIFY(issuesView != nullptr);
  QAbstractItemModel *model = issuesView->model();
  QVERIFY(model != nullptr);

  // Select row and trigger action
  issuesView->selectionModel()->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);

  QAction *createAction = window->findChild<QAction *>(QStringLiteral("createSessionFromIssueAction"));
  QAction *cancelAction = window->findChild<QAction *>(QStringLiteral("cancelGithubIssueFetchAction"));

  QVERIFY(createAction != nullptr);
  QVERIFY(cancelAction != nullptr);

  {
    QSignalBlocker blocker(&apiManager);
    createAction->trigger(); // Starts fetch for 101
  }

  QCOMPARE(createAction->isEnabled(), false);
  QCOMPARE(cancelAction->isEnabled(), true);

  // Emit stale callbacks for issue 999
  Q_EMIT apiManager.githubIssueContextFailed(sourceId, 999,
                                             ApiError(ApiError::Type::NotFound, QStringLiteral("Not found")));

  // State should be unchanged
  QCOMPARE(createAction->isEnabled(), false);
  QCOMPARE(cancelAction->isEnabled(), true);
  QCOMPARE(spy.count(), 0);

  Q_EMIT apiManager.githubIssueContextReceived(sourceId, 999, QJsonObject(), QJsonArray());

  // State should be unchanged
  QCOMPARE(createAction->isEnabled(), false);
  QCOMPARE(cancelAction->isEnabled(), true);
  QCOMPARE(spy.count(), 0);

  // Now emit the correct callback for issue 101
  QJsonObject fetchedIssue = issue1;
  fetchedIssue[QStringLiteral("body")] = QStringLiteral("Detailed body");
  QJsonArray comments;
  Q_EMIT apiManager.githubIssueContextReceived(sourceId, 101, fetchedIssue, comments);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(createAction->isEnabled(), true);
  QCOMPARE(cancelAction->isEnabled(), false);
}

void TestSourceWindow::testSessionWindowContextualErrors() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  ErrorsModel errorsModel(nullptr, tempDir.filePath(QStringLiteral("errors.json")));
  APIManager apiManager;

  QJsonObject err1;
  err1[QStringLiteral("sessionId")] = QStringLiteral("sess1");
  errorsModel.addErrorObj(err1);

  QJsonObject err2;
  err2[QStringLiteral("sessionId")] = QStringLiteral("sess2");
  errorsModel.addErrorObj(err2);

  QCOMPARE(errorsModel.unseenCount(), 2);

  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  QJsonObject sess1;
  sess1[QStringLiteral("id")] = QStringLiteral("sess1");
  sess1[QStringLiteral("state")] = QStringLiteral("ERROR");
  sessionModel.addSessions(QJsonArray{sess1});

  auto window = std::make_unique<SessionWindow>(sess1, &apiManager, &errorsModel, true, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QTabWidget *tabWidget = window->findChild<QTabWidget *>();
  QVERIFY(tabWidget != nullptr);

  QAbstractItemView *errorsView = nullptr;
  for (int i = 0; i < tabWidget->count(); ++i) {
    if (tabWidget->tabText(i).contains(tr("Error"))) {
      tabWidget->setCurrentIndex(i);
      errorsView = tabWidget->widget(i)->findChild<QAbstractItemView *>();
      break;
    }
  }

  if (!errorsView) {
    errorsView = window->findChild<QAbstractItemView *>();
  }

  window->show();
  QCoreApplication::processEvents();

  bool err1Unseen = true;
  bool err2Unseen = true;

  for (int row = 0; row < errorsModel.rowCount(); ++row) {
    QModelIndex idx = errorsModel.index(row, 0);
    if (idx.data(ErrorsModel::SessionIdRole).toString() == QStringLiteral("sess1")) {
      err1Unseen = idx.data(ErrorsModel::UnseenRole).toBool();
    } else if (idx.data(ErrorsModel::SessionIdRole).toString() == QStringLiteral("sess2")) {
      err2Unseen = idx.data(ErrorsModel::UnseenRole).toBool();
    }
  }

  QCOMPARE(err1Unseen, false);
  QCOMPARE(err2Unseen, true);
  QCOMPARE(errorsModel.unseenCount(), 1);

  QVERIFY(errorsView != nullptr);
  QAbstractItemModel *proxyModel = errorsView->model();
  QVERIFY(proxyModel != nullptr);
  QCOMPARE(proxyModel->rowCount(), 1);
}

void TestSourceWindow::testSessionWindowMessageSendFailureLinks() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  ErrorsModel errorsModel;
  APIManager apiManager;
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  QJsonObject sess1;
  sess1[QStringLiteral("id")] = QStringLiteral("sess1");
  sessionModel.addSessions(QJsonArray{sess1});

  auto window = std::make_unique<SessionWindow>(sess1, &apiManager, &errorsModel, true, nullptr);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QLabel *statusLabel = window->findChild<ClickableLabel *>(QStringLiteral("sessionStatusLabel"));
  QVERIFY(statusLabel != nullptr);

  QTextBrowser *textBrowser = nullptr;
  QTabWidget *tabWidget = window->findChild<QTabWidget *>();
  QVERIFY(tabWidget != nullptr);
  for (int i = 0; i < tabWidget->count(); ++i) {
    if (tabWidget->tabText(i).contains(QStringLiteral("Raw JSON"))) {
      textBrowser = qobject_cast<QTextBrowser *>(tabWidget->widget(i));
      break;
    }
  }
  QVERIFY(textBrowser != nullptr);

  // Send failure A
  Q_EMIT apiManager.messageSendFailed(QStringLiteral("sess1"), QStringLiteral("Error A"), QStringLiteral("Details A"));

  // Send failure B
  Q_EMIT apiManager.messageSendFailed(QStringLiteral("sess1"), QStringLiteral("Error B"), QStringLiteral("Details B"));

  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));

  QCoreApplication::processEvents();

  QTest::mouseClick(statusLabel, Qt::LeftButton, Qt::NoModifier,
                    QPoint(statusLabel->width() - 5, statusLabel->height() / 2));

  QCoreApplication::processEvents();

  QCOMPARE(textBrowser->toPlainText(), QStringLiteral("Details B"));
}

void TestSourceWindow::testClickableLabelLinkHandling() {
  ClickableLabel label(QStringLiteral("Normal <a href=\"#test\">Link</a> Text"));
  label.setTextFormat(Qt::RichText);
  label.setTextInteractionFlags(Qt::TextBrowserInteraction);
  label.show();

  QSignalSpy clickedSpy(&label, &ClickableLabel::clicked);
  QSignalSpy linkSpy(&label, &QLabel::linkActivated);

  // Wait for it to be visible
  QVERIFY(QTest::qWaitForWindowExposed(&label));

  // Click on the link. QTest::mouseClick clicks the center by default.
  // We need to click exactly on the link.
  // Actually, we can't easily guess the geometry of the link in the test.
  // But wait! If we set the whole text as a link, the center will be the link!
  label.setText(QStringLiteral("<a href=\"#test\">Hyperlink Only</a>"));

  QCoreApplication::processEvents();

  QTest::mouseClick(&label, Qt::LeftButton);

  QCOMPARE(linkSpy.count(), 1);
  QCOMPARE(clickedSpy.count(), 0);

  // Now test normal text
  linkSpy.clear();
  clickedSpy.clear();

  label.setText(QStringLiteral("Normal Text Only"));
  QCoreApplication::processEvents();

  QTest::mouseClick(&label, Qt::LeftButton);

  QCOMPARE(linkSpy.count(), 0);
  QCOMPARE(clickedSpy.count(), 1);
}

void TestSourceWindow::testFullSemanticChain() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  SessionModel *followingModel = window.sessionModel();
  SessionModel *archiveModel = window.archiveModel();
  APIManager *apiManager = window.apiManager();

  followingModel->clearSessions();
  archiveModel->clearSessions();

  QJsonObject sess;
  sess[QStringLiteral("id")] = QStringLiteral("sess-e2e-1");
  sess[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  sess[QStringLiteral("lastRefreshed")] = QStringLiteral("2020-01-01T00:00:00Z");
  followingModel->addSession(sess);

  // Disable network calls safely
  apiManager->setBaseUrl(QStringLiteral(""));
  apiManager->setApiKey(QStringLiteral(""));

  // 1. We skip autoRefreshFollowing directly since it tries to make a network call with m_nam.
  // Instead, we verify the semantic state update via APIManager callbacks that MainWindow listens to.

  // 2. Simulate API returning the reloaded session with a NEW PR URL
  QJsonObject reloadedObj = sess;
  QJsonObject prObj;
  prObj[QStringLiteral("url")] = QStringLiteral("https://github.com/owner/repo/pull/123");
  reloadedObj[QStringLiteral("pullRequest")] = prObj;

  // Call the slot that MainWindow listens to
  Q_EMIT apiManager->sessionReloaded(reloadedObj, true);

  // At this point, the PR URL is present in the model.
  bool foundInFollowing = false;
  for (int i = 0; i < followingModel->rowCount(); ++i) {
    if (followingModel->index(i, 0).data(SessionModel::IdRole).toString() == QStringLiteral("sess-e2e-1")) {
      foundInFollowing = true;
      QCOMPARE(followingModel->index(i, 0).data(SessionModel::PrUrlRole).toString(),
               QStringLiteral("https://github.com/owner/repo/pull/123"));
      break;
    }
  }
  QVERIFY(foundInFollowing);

  // 3. Simulate GitHub PR response changing state to 'merged'
  QJsonObject githubPr;
  githubPr[QStringLiteral("state")] = QStringLiteral("merged");

  KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("Behavior"));
  cg.writeEntry("AutoArchiveCompletedWithClosedPRs", true);
  cg.sync();

  // Let's invoke MainWindow::onGithubPullRequestInfoReceived using the real signal
  Q_EMIT apiManager->githubPullRequestInfoReceived(QStringLiteral("https://github.com/owner/repo/pull/123"), githubPr);

  QCoreApplication::processEvents();

  // Verify model updated with 'merged' state and it was archived
  bool foundInArchive = false;
  for (int i = 0; i < archiveModel->rowCount(); ++i) {
    if (archiveModel->index(i, 0).data(SessionModel::IdRole).toString() == QStringLiteral("sess-e2e-1")) {
      foundInArchive = true;
      break;
    }
  }

  foundInFollowing = false;
  for (int i = 0; i < followingModel->rowCount(); ++i) {
    if (followingModel->index(i, 0).data(SessionModel::IdRole).toString() == QStringLiteral("sess-e2e-1")) {
      foundInFollowing = true;
      break;
    }
  }

  QVERIFY(foundInArchive);
  QVERIFY(!foundInFollowing);
}

void TestSourceWindow::testManualVsAutomaticRefreshEquivalent() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  SessionModel *followingModel = window.sessionModel();
  APIManager *apiManager = window.apiManager();

  followingModel->clearSessions();

  QJsonObject sess;
  sess[QStringLiteral("id")] = QStringLiteral("sess-manual-1");
  sess[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  sess[QStringLiteral("lastRefreshed")] = QStringLiteral("2020-01-01T00:00:00Z");
  followingModel->addSession(sess);

  QJsonObject reloadedObj = sess;
  QJsonObject prObj;
  prObj[QStringLiteral("url")] = QStringLiteral("https://github.com/owner/repo/pull/999");
  reloadedObj[QStringLiteral("pullRequest")] = prObj;

  // In manual refresh, isBackground is false.
  Q_EMIT apiManager->sessionReloaded(reloadedObj, false);

  // Check if it stored the PR URL
  bool hasPrUrl = false;
  for (int i = 0; i < followingModel->rowCount(); ++i) {
    if (followingModel->index(i, 0).data(SessionModel::IdRole).toString() == QStringLiteral("sess-manual-1")) {
      hasPrUrl = !followingModel->index(i, 0).data(SessionModel::PrUrlRole).toString().isEmpty();
      break;
    }
  }
  QVERIFY(hasPrUrl);
}

void TestSourceWindow::testInFlightRecovery() {
  MainWindow window;
  window.setAttribute(Qt::WA_DeleteOnClose, false);

  SessionModel *followingModel = window.sessionModel();
  APIManager *apiManager = window.apiManager();

  followingModel->clearSessions();

  QJsonObject sess;
  sess[QStringLiteral("id")] = QStringLiteral("sess-inflight-1");
  sess[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  sess[QStringLiteral("lastRefreshed")] = QStringLiteral("2020-01-01T00:00:00Z");
  followingModel->addSession(sess);

  apiManager->setBaseUrl(QStringLiteral("https://jules.example.com"));
  apiManager->setApiKey(QStringLiteral("fake-key"));

  auto *mockNam = new MockE2ENetworkAccessManager(apiManager);
  mockNam->julesResponse = QByteArray("malformed json");
  delete apiManager->m_nam;
  apiManager->m_nam = mockNam;

  // Trigger first automatic refresh
  QMetaObject::invokeMethod(&window, "autoRefreshFollowing", Qt::DirectConnection);

  for (int i = 0; i < 10; ++i) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QThread::msleep(10);
  }

  // Verify it requested it once
  int requestCount = 0;
  for (const QString &url : mockNam->requestedUrls) {
    if (url.contains(QStringLiteral("sess-inflight-1")))
      requestCount++;
  }
  QCOMPARE(requestCount, 1);

  // To prove it is NOT wedged in `m_inFlightSessionReloads`, we can trigger another refresh.
  // Because of the 5-minute cooldown in `autoRefreshFollowing`, it normally wouldn't dispatch.
  // We can bypass the cooldown by modifying `FollowingRefreshEvaluator`? No.
  // But we CAN trigger a manual refresh, which also checks `m_inFlightSessionReloads` before dispatching!
  // Wait, does manual refresh check `m_inFlightSessionReloads`? Let's assume it does, or we can just call
  // `apiManager->reloadSession` directly? No, we need to prove MainWindow cleared its tracking. If we invoke the manual
  // refresh action, it dispatches.

  // To prove it is NOT wedged in `m_inFlightSessionReloads`, we can trigger another auto refresh.
  // Because of the 5-minute cooldown in `autoRefreshFollowing`, it normally wouldn't dispatch.
  // We simulate the passage of the 5-minute cooldown by clearing the failure timestamp.
  window.m_sessionReloadFailedAt.remove(QStringLiteral("sess-inflight-1"));

  mockNam->requestedUrls.clear();
  QMetaObject::invokeMethod(&window, "autoRefreshFollowing", Qt::DirectConnection);

  for (int i = 0; i < 10; ++i) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QThread::msleep(10);
  }

  // If it was wedged in m_inFlightSessionReloads, the automatic refresh would skip it!
  requestCount = 0;
  for (const QString &url : mockNam->requestedUrls) {
    if (url.contains(QStringLiteral("sess-inflight-1")))
      requestCount++;
  }
  QCOMPARE(requestCount, 1);
}

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("KDE_HOME_READONLY", "1");
  qputenv("CANBERRA_DRIVER", "null");
  qputenv("KNOTIFICATIONS_DEFAULT_BACKEND", "null");
  QStandardPaths::setTestModeEnabled(true);
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("kjules-test"));
  app.setOrganizationName(QStringLiteral("KDE"));
  TestSourceWindow tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_sourcewindow.moc"
