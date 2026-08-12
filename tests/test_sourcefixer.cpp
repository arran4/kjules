#include "../src/sessionrequestbuilder.h"
#include "../src/sourcefixer.h"

#include <QtTest>

class SourceFixerTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void extractsAllRequestShapes();
  void remapsNestedRequestWithoutLosingBranch();
  void remapsStoredFlatErrorThroughApiRequest();
  void remapsEmbeddedRecoveryCopies();
  void prefersInsertedProviderSegment();
  void requiresExactResourceNameForIdentity();
};

void SourceFixerTest::extractsAllRequestShapes() {
  QCOMPARE(SourceFixer::source({{QStringLiteral("source"), QStringLiteral("arran4/blog")}}),
           QStringLiteral("arran4/blog"));
  QCOMPARE(
      SourceFixer::source({{QStringLiteral("sourceContext"),
                            QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/github/arran4/blog")}}}}),
      QStringLiteral("sources/github/arran4/blog"));
  QCOMPARE(SourceFixer::source({{QStringLiteral("request"),
                                 QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/arran4/blog")}}}}),
           QStringLiteral("sources/arran4/blog"));
}

void SourceFixerTest::remapsNestedRequestWithoutLosingBranch() {
  const QJsonObject input{
      {QStringLiteral("request"),
       QJsonObject{{QStringLiteral("prompt"), QStringLiteral("Task")},
                   {QStringLiteral("sourceContext"),
                    QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/arran4/blog")},
                                {QStringLiteral("githubRepoContext"),
                                 QJsonObject{{QStringLiteral("startingBranch"), QStringLiteral("main")}}}}}}}};
  const QJsonObject result = SourceFixer::remap(input, QStringLiteral("sources/github/arran4/blog"));
  const QJsonObject context =
      result.value(QStringLiteral("request")).toObject().value(QStringLiteral("sourceContext")).toObject();
  QCOMPARE(context.value(QStringLiteral("source")).toString(), QStringLiteral("sources/github/arran4/blog"));
  QCOMPARE(
      context.value(QStringLiteral("githubRepoContext")).toObject().value(QStringLiteral("startingBranch")).toString(),
      QStringLiteral("main"));
}

void SourceFixerTest::remapsStoredFlatErrorThroughApiRequest() {
  const QJsonObject storedError{
      {QStringLiteral("request"),
       QJsonObject{{QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")},
                   {QStringLiteral("prompt"), QStringLiteral("https://github.com/block/buzz")},
                   {QStringLiteral("source"), QStringLiteral("arran4/arrans_overlay")},
                   {QStringLiteral("startingBranch"), QStringLiteral("main")}}}};

  const QJsonObject remapped = SourceFixer::remap(storedError, QStringLiteral("sources/github/arran4/arrans_overlay"));
  const QJsonObject storedRequest = remapped.value(QStringLiteral("request")).toObject();
  QCOMPARE(storedRequest.value(QStringLiteral("source")).toString(),
           QStringLiteral("sources/github/arran4/arrans_overlay"));

  const QJsonObject apiRequest = SessionRequestBuilder::createSession(storedRequest);
  QCOMPARE(apiRequest.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString(),
           QStringLiteral("sources/github/arran4/arrans_overlay"));
}

void SourceFixerTest::remapsEmbeddedRecoveryCopies() {
  const QJsonObject input{
      {QStringLiteral("source"), QStringLiteral("arran4/arrans_overlay")},
      {QStringLiteral("_kjules_requeue_item"),
       QJsonObject{{QStringLiteral("requestData"),
                    QJsonObject{{QStringLiteral("source"), QStringLiteral("arran4/arrans_overlay")}}}}},
      {QStringLiteral("_kjules_requeue_err_data"),
       QJsonObject{{QStringLiteral("request"),
                    QJsonObject{{QStringLiteral("source"), QStringLiteral("arran4/arrans_overlay")}}}}}};
  const QJsonObject remapped = SourceFixer::remap(input, QStringLiteral("sources/github/arran4/arrans_overlay"));

  QCOMPARE(remapped.value(QStringLiteral("source")).toString(), QStringLiteral("sources/github/arran4/arrans_overlay"));
  QCOMPARE(remapped.value(QStringLiteral("_kjules_requeue_item"))
               .toObject()
               .value(QStringLiteral("requestData"))
               .toObject()
               .value(QStringLiteral("source"))
               .toString(),
           QStringLiteral("sources/github/arran4/arrans_overlay"));
  QCOMPARE(remapped.value(QStringLiteral("_kjules_requeue_err_data"))
               .toObject()
               .value(QStringLiteral("request"))
               .toObject()
               .value(QStringLiteral("source"))
               .toString(),
           QStringLiteral("sources/github/arran4/arrans_overlay"));
}

void SourceFixerTest::prefersInsertedProviderSegment() {
  const QStringList sources{QStringLiteral("sources/github/arran4/blog-old"),
                            QStringLiteral("sources/github/arran4/blog"),
                            QStringLiteral("sources/github/someone/blog")};
  QCOMPARE(SourceFixer::bestMatch(QStringLiteral("sources/arran4/blog"), sources),
           QStringLiteral("sources/github/arran4/blog"));
}

void SourceFixerTest::requiresExactResourceNameForIdentity() {
  QVERIFY(!SourceFixer::sameSource(QStringLiteral("arran4/blog"), QStringLiteral("sources/arran4/blog")));
  QVERIFY(SourceFixer::sameSource(QStringLiteral("sources/arran4/blog"), QStringLiteral("sources/arran4/blog")));
  QVERIFY(!SourceFixer::sameSource(QStringLiteral("arran4/blog"), QStringLiteral("sources/github/arran4/blog")));
}

QTEST_MAIN(SourceFixerTest)
#include "test_sourcefixer.moc"
