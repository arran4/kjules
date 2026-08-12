#include "../src/sourcefixer.h"

#include <QtTest>

class SourceFixerTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void extractsAllRequestShapes();
  void remapsNestedRequestWithoutLosingBranch();
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
