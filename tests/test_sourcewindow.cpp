#include <QComboBox>
#include <QPushButton>
#include "../src/api/apierror.h"
#include "../src/apimanager.h"
#include "../src/blockedtreemodel.h"
#include "../src/errorsmodel.h"
#include "../src/queuemodel.h"
#include "../src/sessionmodel.h"
#include "../src/sessionswidget.h"
#include "../src/sessionswindow.h"
#include "../src/sourcemodel.h"
#include "../src/sourcewindow.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeView>
#include <QtTest>

class TestSourceWindow : public QObject {
  Q_OBJECT

private Q_SLOTS:
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
  void testGithubIssuesAndPRsTabs();
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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
  ErrorsModel errorsModel;
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

void TestSourceWindow::testGithubIssuesAndPRsTabs() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SourceModel sourceModel(nullptr, SourceModel::StorageMode::InMemory);
  SessionModel sessionModel(tempDir.filePath(QStringLiteral("sessions.json")));
  SessionModel archiveModel(tempDir.filePath(QStringLiteral("archive.json")));
  QueueModel queueModel(nullptr, tempDir.filePath(QStringLiteral("queue.json")));
  ErrorsModel errorsModel;
  BlockedTreeModel blockedTreeModel(&sourceModel, &queueModel);

  QString sourceId = QStringLiteral("sources/github/kde/kjules");

  QJsonObject srcObj;
  srcObj[QStringLiteral("name")] = sourceId;
  QJsonObject ghRepo;
  ghRepo[QStringLiteral("owner")] = QStringLiteral("kde");
  ghRepo[QStringLiteral("repo")] = QStringLiteral("kjules");
  srcObj[QStringLiteral("githubRepo")] = ghRepo;
  sourceModel.addSources(QJsonArray{srcObj});

  APIManager apiManager;
  // Mock valid token so canConnectGithub is true
  apiManager.setGithubToken(QStringLiteral("fake_token"));

  auto window = std::make_unique<SourceWindow>(sourceId, &sourceModel, &sessionModel, &archiveModel, &queueModel,
                                               &errorsModel, &blockedTreeModel, &apiManager);
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  QTabWidget *tabWidget = window->findChild<QTabWidget *>();
  QVERIFY(tabWidget != nullptr);

  bool foundIssuesTab = false;
  bool foundPRsTab = false;
  for (int i = 0; i < tabWidget->count(); ++i) {
    if (tabWidget->tabText(i) == tr("GitHub Issues")) {
      foundIssuesTab = true;
    } else if (tabWidget->tabText(i) == tr("GitHub PRs")) {
      foundPRsTab = true;
    }
  }

  QVERIFY(foundIssuesTab);
  QVERIFY(foundPRsTab);

  QJsonArray mixedIssues;
  QJsonObject issue1;
  issue1[QStringLiteral("number")] = 1;
  issue1[QStringLiteral("title")] = QStringLiteral("Issue 1");
  issue1[QStringLiteral("state")] = QStringLiteral("open");
  QJsonObject issue2;
  issue2[QStringLiteral("number")] = 2;
  issue2[QStringLiteral("title")] = QStringLiteral("Issue 2");
  issue2[QStringLiteral("state")] = QStringLiteral("open");
  issue2[QStringLiteral("pull_request")] = QJsonObject();
  QJsonObject issue3;
  issue3[QStringLiteral("number")] = 3;
  issue3[QStringLiteral("title")] = QStringLiteral("Closed Issue");
  issue3[QStringLiteral("state")] = QStringLiteral("closed");
  mixedIssues.append(issue1);
  mixedIssues.append(issue2);
  mixedIssues.append(issue3);

  QMetaObject::invokeMethod(window.get(), "onGithubIssuesReceived", Q_ARG(QString, sourceId),
                            Q_ARG(QJsonArray, mixedIssues));

  QModelIndex idx = sourceModel.index(0, 0);
  QJsonObject updated = sourceModel.data(idx, SourceModel::RawDataRole).toJsonObject();
  QJsonArray cachedIssues = updated.value(QStringLiteral("local_githubIssues")).toArray();

  QCOMPARE(cachedIssues.size(), 1);
  QCOMPARE(cachedIssues.at(0).toObject().value(QStringLiteral("number")).toInt(), 1);

  // Test Context received signal creates session initial data properly
  QSignalSpy spy(window.get(), &SourceWindow::newSessionFromIssueRequested);
  QJsonArray comments;
  QJsonObject comment1;
  comment1[QStringLiteral("body")] = QStringLiteral("This is a comment");
  comments.append(comment1);

  QMetaObject::invokeMethod(window.get(), "onGithubIssueContextReceived", Q_ARG(QString, sourceId), Q_ARG(int, 1),
                            Q_ARG(QJsonObject, issue1), Q_ARG(QJsonArray, comments));

  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), sourceId);
  QJsonObject initialData = args.at(1).toJsonObject();
  QVERIFY(initialData.contains(QStringLiteral("prompt")));
  QString prompt = initialData.value(QStringLiteral("prompt")).toString();
  QVERIFY(prompt.contains(QStringLiteral("Implement GitHub issue #1: Issue 1")));
  QVERIFY(prompt.contains(QStringLiteral("This is a comment")));
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
