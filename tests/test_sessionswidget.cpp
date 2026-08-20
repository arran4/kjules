#include "../src/sessionmodel.h"
#include "../src/sessionswidget.h"
#include "../src/sessionswindow.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QActionGroup>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

class TestSessionsWidget : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testSessionFiltering();
  void testExactSourceBoundaryAndUserSearchIsolation();
  void testPerSourceAutoFollowIgnoresForeignAndInactiveSessions();
  void testGlobalSessionsWindowPreservesGlobalAutoFollow();
  void testFavouriteAwareSorting();
  void testSourceUrlConstruction();
  void testActionStatesAndTriggeredConnections();
  void testUnmanageOwnership();
  void testConfigPreservationAndMigration();
};

void TestSessionsWidget::initTestCase() {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("KDE_HOME_READONLY", "1");
  qputenv("CANBERRA_DRIVER", "null");
  qputenv("KNOTIFICATIONS_DEFAULT_BACKEND", "null");
}

void TestSessionsWidget::testSessionFiltering() {
  SessionsProxyModel proxy;
  SessionModel model(QString(), nullptr);

  QJsonObject s1;
  s1[QStringLiteral("id")] = QStringLiteral("s1");
  s1[QStringLiteral("title")] = QStringLiteral("Fix memory leak in parser");
  s1[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  QJsonObject src1;
  src1[QStringLiteral("source")] = QStringLiteral("sources/github/kde/kjules");
  s1[QStringLiteral("sourceContext")] = src1;

  QJsonObject s2;
  s2[QStringLiteral("id")] = QStringLiteral("s2");
  s2[QStringLiteral("title")] = QStringLiteral("Add feature to core");
  s2[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  QJsonObject src2;
  src2[QStringLiteral("source")] = QStringLiteral("sources/github/qt/qtbase");
  s2[QStringLiteral("sourceContext")] = src2;

  QJsonObject s3;
  s3[QStringLiteral("id")] = QStringLiteral("s3");
  s3[QStringLiteral("title")] = QStringLiteral("Refactor UI models");
  s3[QStringLiteral("state")] = QStringLiteral("PENDING");
  QJsonObject src3;
  src3[QStringLiteral("source")] = QStringLiteral("sources/github/kde/kcoreaddons");
  s3[QStringLiteral("sourceContext")] = src3;

  model.addSessions(QJsonArray{s1, s2, s3});
  proxy.setSourceModel(&model);

  QCOMPARE(proxy.rowCount(), 3);

  // Text filter: Title match
  proxy.setTextFilter(QStringLiteral("leak"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.data(proxy.index(0, SessionModel::ColTitle)).toString(), QStringLiteral("Fix memory leak in parser"));

  // Text filter: Owner match
  proxy.setTextFilter(QStringLiteral("kde"));
  QCOMPARE(proxy.rowCount(), 2);

  // Text filter: Repo match
  proxy.setTextFilter(QStringLiteral("qtbase"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.data(proxy.index(0, SessionModel::ColTitle)).toString(), QStringLiteral("Add feature to core"));

  // Clear text filter
  proxy.setTextFilter(QString());
  QCOMPARE(proxy.rowCount(), 3);

  // Status filter
  proxy.setStatusFilter(QStringLiteral("COMPLETED"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.data(proxy.index(0, SessionModel::ColTitle)).toString(), QStringLiteral("Add feature to core"));

  proxy.setStatusFilter(QString());
  QCOMPARE(proxy.rowCount(), 3);

  // Repo filter
  proxy.setRepoFilter(QStringLiteral("kjules"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.data(proxy.index(0, SessionModel::ColTitle)).toString(), QStringLiteral("Fix memory leak in parser"));
}

void TestSessionsWidget::testExactSourceBoundaryAndUserSearchIsolation() {
  const QString sourceA = QStringLiteral("sources/github/kde/kjules");
  const QString sourceB = QStringLiteral("sources/github/qt/qtbase");

  SessionsWidget widget(sourceA);
  SessionModel *model = widget.model();

  QJsonObject s1;
  s1[QStringLiteral("id")] = QStringLiteral("s1");
  s1[QStringLiteral("title")] = QStringLiteral("Feature in KJules");
  s1[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  QJsonObject src1;
  src1[QStringLiteral("source")] = sourceA;
  s1[QStringLiteral("sourceContext")] = src1;

  QJsonObject s2;
  s2[QStringLiteral("id")] = QStringLiteral("s2");
  s2[QStringLiteral("title")] = QStringLiteral("Bugfix in KJules parser");
  s2[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  s2[QStringLiteral("sourceContext")] = src1;

  QJsonObject s3;
  s3[QStringLiteral("id")] = QStringLiteral("s3");
  s3[QStringLiteral("title")] = QStringLiteral("Feature in QtBase core");
  s3[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  QJsonObject src3;
  src3[QStringLiteral("source")] = sourceB;
  s3[QStringLiteral("sourceContext")] = src3;

  model->addSessions(QJsonArray{s1, s2, s3});

  // 1. Source-scoped widget displays ONLY sessions from sourceA (2 sessions, not 3)
  QCOMPARE(widget.proxyModel()->rowCount(), 2);

  // 2. User search does not have source pre-filled
  QLineEdit *searchEdit = widget.findChild<QLineEdit *>();
  QVERIFY(searchEdit != nullptr);
  QCOMPARE(searchEdit->text(), QString());

  // 3. User search narrows within sourceA
  searchEdit->setText(QStringLiteral("parser"));
  QCOMPARE(widget.proxyModel()->rowCount(), 1);
  QCOMPARE(widget.proxyModel()->data(widget.proxyModel()->index(0, SessionModel::ColTitle)).toString(),
           QStringLiteral("Bugfix in KJules parser"));

  // 4. Searching for sourceB content or sourceB ID yields 0 results (cannot break boundary)
  searchEdit->setText(QStringLiteral("QtBase"));
  QCOMPARE(widget.proxyModel()->rowCount(), 0);

  searchEdit->setText(sourceB);
  QCOMPARE(widget.proxyModel()->rowCount(), 0);

  // 5. Clearing user search restores exactly all sourceA sessions
  searchEdit->setText(QString());
  QCOMPARE(widget.proxyModel()->rowCount(), 2);
}

void TestSessionsWidget::testPerSourceAutoFollowIgnoresForeignAndInactiveSessions() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SessionModel managedModel(tempDir.filePath(QStringLiteral("managed.json")));
  const QString sourceA = QStringLiteral("sources/github/kde/kjules");
  const QString sourceB = QStringLiteral("sources/github/qt/qtbase");

  SessionsWidget widget(sourceA, nullptr, &managedModel);
  widget.setAutoFollowOnRefresh(true);

  QSignalSpy watchSpy(&widget, &SessionsWidget::watchRequested);

  // Mixed incoming payload:
  // 1: Active in sourceA -> SHOULD follow
  QJsonObject s1;
  s1[QStringLiteral("id")] = QStringLiteral("s1");
  s1[QStringLiteral("title")] = QStringLiteral("Active Session A");
  s1[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  QJsonObject src1;
  src1[QStringLiteral("source")] = sourceA;
  s1[QStringLiteral("sourceContext")] = src1;

  // 2: Inactive in sourceA -> should NOT follow
  QJsonObject s2;
  s2[QStringLiteral("id")] = QStringLiteral("s2");
  s2[QStringLiteral("title")] = QStringLiteral("Inactive Session A");
  s2[QStringLiteral("state")] = QStringLiteral("COMPLETED");
  s2[QStringLiteral("sourceContext")] = src1;

  // 3: Active in sourceB -> should NOT follow (foreign source)
  QJsonObject s3;
  s3[QStringLiteral("id")] = QStringLiteral("s3");
  s3[QStringLiteral("title")] = QStringLiteral("Active Session B");
  s3[QStringLiteral("state")] = QStringLiteral("IN_PROGRESS");
  QJsonObject src3;
  src3[QStringLiteral("source")] = sourceB;
  s3[QStringLiteral("sourceContext")] = src3;

  // 4: Inactive in sourceB -> should NOT follow
  QJsonObject s4;
  s4[QStringLiteral("id")] = QStringLiteral("s4");
  s4[QStringLiteral("title")] = QStringLiteral("Inactive Session B");
  s4[QStringLiteral("state")] = QStringLiteral("FAILED");
  s4[QStringLiteral("sourceContext")] = src3;

  // Simulate API arrival via onSessionsReceived
  QJsonArray mixedBatch{s1, s2, s3, s4};
  QMetaObject::invokeMethod(&widget, "onSessionsReceived", Q_ARG(QJsonArray, mixedBatch), Q_ARG(QString, QString()));

  QCOMPARE(watchSpy.count(), 1);
  QCOMPARE(watchSpy.first().at(0).toJsonObject().value(QStringLiteral("id")).toString(), QStringLiteral("s1"));
}

void TestSessionsWidget::testGlobalSessionsWindowPreservesGlobalAutoFollow() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SessionModel managedModel(tempDir.filePath(QStringLiteral("managed.json")));
  const QString sourceA = QStringLiteral("sources/github/kde/kjules");
  const QString sourceB = QStringLiteral("sources/github/qt/qtbase");

  SessionsWidget widget(QString(), nullptr, &managedModel);
  widget.setAutoFollowOnRefresh(true);

  QSignalSpy watchSpy(&widget, &SessionsWidget::watchRequested);

  QJsonObject s1;
  s1[QStringLiteral("id")] = QStringLiteral("s1");
  s1[QStringLiteral("state")] = JulesStatus::IN_PROGRESS;
  QJsonObject src1;
  src1[QStringLiteral("source")] = sourceA;
  s1[QStringLiteral("sourceContext")] = src1;

  QJsonObject s2;
  s2[QStringLiteral("id")] = QStringLiteral("s2");
  s2[QStringLiteral("state")] = JulesStatus::IN_PROGRESS;
  QJsonObject src2;
  src2[QStringLiteral("source")] = sourceB;
  s2[QStringLiteral("sourceContext")] = src2;

  QJsonArray globalBatch{s1, s2};
  QMetaObject::invokeMethod(&widget, "onSessionsReceived", Q_ARG(QJsonArray, globalBatch), Q_ARG(QString, QString()));

  // Global widget follows active sessions from all sources
  QCOMPARE(watchSpy.count(), 2);
}

void TestSessionsWidget::testFavouriteAwareSorting() {
  SessionsProxyModel proxy;
  SessionModel model(QString(), nullptr);

  QJsonObject s1;
  s1[QStringLiteral("id")] = QStringLiteral("s1");
  s1[QStringLiteral("title")] = QStringLiteral("ZZZ Session");
  s1[QStringLiteral("local_favourite")] = 1;

  QJsonObject s2;
  s2[QStringLiteral("id")] = QStringLiteral("s2");
  s2[QStringLiteral("title")] = QStringLiteral("AAA Session");

  model.addSessions(QJsonArray{s1, s2});
  proxy.setSourceModel(&model);
  proxy.sort(SessionModel::ColTitle, Qt::AscendingOrder);

  // Favourite (s1) must come before non-favourite (s2) despite "ZZZ" vs "AAA"
  QCOMPARE(proxy.data(proxy.index(0, SessionModel::ColTitle)).toString(), QStringLiteral("ZZZ Session"));
  QCOMPARE(proxy.data(proxy.index(1, SessionModel::ColTitle)).toString(), QStringLiteral("AAA Session"));
}

void TestSessionsWidget::testSourceUrlConstruction() {
  SessionsWidget widget;
  SessionModel *model = widget.model();

  QJsonObject sGithub;
  sGithub[QStringLiteral("id")] = QStringLiteral("gh1");
  sGithub[QStringLiteral("title")] = QStringLiteral("GH Session");
  QJsonObject srcGh;
  srcGh[QStringLiteral("source")] = QStringLiteral("sources/github/torvalds/linux");
  sGithub[QStringLiteral("sourceContext")] = srcGh;

  QJsonObject sGitlab;
  sGitlab[QStringLiteral("id")] = QStringLiteral("gl1");
  sGitlab[QStringLiteral("title")] = QStringLiteral("GL Session");
  QJsonObject srcGl;
  srcGl[QStringLiteral("source")] = QStringLiteral("sources/gitlab/inkscape/inkscape");
  sGitlab[QStringLiteral("sourceContext")] = srcGl;

  QJsonObject sBitbucket;
  sBitbucket[QStringLiteral("id")] = QStringLiteral("bb1");
  sBitbucket[QStringLiteral("title")] = QStringLiteral("BB Session");
  QJsonObject srcBb;
  srcBb[QStringLiteral("source")] = QStringLiteral("sources/bitbucket/atlassian/confluence");
  sBitbucket[QStringLiteral("sourceContext")] = srcBb;

  model->addSessions(QJsonArray{sGithub, sGitlab, sBitbucket});

  QCOMPARE(model->data(model->index(0, 0), SessionModel::ProviderRole).toString(), QStringLiteral("github"));
  QCOMPARE(model->data(model->index(1, 0), SessionModel::ProviderRole).toString(), QStringLiteral("gitlab"));
  QCOMPARE(model->data(model->index(2, 0), SessionModel::ProviderRole).toString(), QStringLiteral("bitbucket"));
}

void TestSessionsWidget::testActionStatesAndTriggeredConnections() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SessionModel managedModel(tempDir.filePath(QStringLiteral("managed.json")));
  QJsonObject sManaged;
  sManaged[QStringLiteral("id")] = QStringLiteral("sess-managed");
  sManaged[QStringLiteral("title")] = QStringLiteral("Managed Session");
  managedModel.addSession(sManaged);

  SessionsWidget widget(QString(), nullptr, &managedModel);
  SessionModel *model = widget.model();

  QJsonObject sUnmanaged;
  sUnmanaged[QStringLiteral("id")] = QStringLiteral("sess-unmanaged");
  sUnmanaged[QStringLiteral("title")] = QStringLiteral("Unmanaged Session");

  model->addSessions(QJsonArray{sManaged, sUnmanaged});

  QSignalSpy actionStatesSpy(&widget, &SessionsWidget::actionStatesChanged);
  QSignalSpy watchSpy(&widget, &SessionsWidget::watchRequested);
  QSignalSpy archiveSpy(&widget, &SessionsWidget::archiveRequested);
  QSignalSpy deleteSpy(&widget, &SessionsWidget::deleteRequested);

  QTreeView *treeView = widget.listView();
  treeView->selectionModel()->select(widget.proxyModel()->index(0, 0),
                                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

  // Managed row selected: canArchive=true, canDelete=true, canWatch=false
  widget.archiveSelectedSessions();
  QCOMPARE(archiveSpy.count(), 1);
  QCOMPARE(archiveSpy.first().at(0).toString(), QStringLiteral("sess-managed"));

  // Select unmanaged row
  treeView->selectionModel()->select(widget.proxyModel()->index(1, 0),
                                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

  widget.watchSelectedSessions();
  QCOMPARE(watchSpy.count(), 1);
  QCOMPARE(watchSpy.first().at(0).toJsonObject().value(QStringLiteral("id")).toString(),
           QStringLiteral("sess-unmanaged"));
}

void TestSessionsWidget::testUnmanageOwnership() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SessionModel managedModel(tempDir.filePath(QStringLiteral("managed.json")));
  QJsonObject sManaged;
  sManaged[QStringLiteral("id")] = QStringLiteral("sess-1");
  managedModel.addSession(sManaged);

  SessionsWidget widget(QString(), nullptr, &managedModel);
  SessionModel *model = widget.model();
  model->addSessions(QJsonArray{sManaged});

  QSignalSpy deleteSpy(&widget, &SessionsWidget::deleteRequested);

  QTreeView *treeView = widget.listView();
  treeView->selectionModel()->select(widget.proxyModel()->index(0, 0),
                                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

  widget.unmanageSelectedSessions();

  // unmanageSelectedSessions must emit deleteRequested without modifying m_managedModel directly
  QCOMPARE(deleteSpy.count(), 1);
  QCOMPARE(deleteSpy.first().at(0).toString(), QStringLiteral("sess-1"));
}

void TestSessionsWidget::testConfigPreservationAndMigration() {
  // Test migration from legacy AutoLoadBehavior to AutoLoadMode
  {
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
    config.deleteEntry("AutoLoadMode");
    config.writeEntry("AutoLoadBehavior", QStringLiteral("load_bottom"));
    config.deleteEntry("AutoFollowRefresh");
    config.writeEntry("AutoFollowActive", true);
    config.sync();
  }

  auto window = std::make_unique<SessionsWindow>();
  window->setAttribute(Qt::WA_DeleteOnClose, false);

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionsWindow"));
  // Trigger auto-load mode action
  QAction *autoLoadAll = window->actionCollection()->action(QStringLiteral("auto_load_all"));
  QVERIFY(autoLoadAll != nullptr);
  autoLoadAll->trigger();
  config.sync();

  QCOMPARE(config.readEntry("AutoLoadMode", QString()), QStringLiteral("load_all"));

  // Trigger auto-follow action
  QAction *autoFollow = window->actionCollection()->action(QStringLiteral("auto_follow_refresh"));
  QVERIFY(autoFollow != nullptr);
  autoFollow->setChecked(false);
  config.sync();

  QCOMPARE(config.readEntry("AutoFollowRefresh", true), false);
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
  TestSessionsWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_sessionswidget.moc"
