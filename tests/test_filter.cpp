#include "../src/filtereditor.h"
#include "../src/filterparser.h"
#include <QtTest>

class TestFilter : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testApplyQuickFilter() {
    QString base = QStringLiteral("=repo:test");
    QString updated = FilterEditor::applyQuickFilter(
        base, QStringLiteral("owner"), QStringLiteral("jules"), false);
    QVERIFY(updated.contains(QStringLiteral("owner:jules")));
    QVERIFY(updated.contains(QStringLiteral("repo:test")));
    QVERIFY(updated.contains(QStringLiteral("AND")));

    QString hideUpdated = FilterEditor::applyQuickFilter(
        base, QStringLiteral("owner"), QStringLiteral("jules"), true);
    QVERIFY(hideUpdated.contains(QStringLiteral("NOT owner:jules")));
  }

  void testMergeFilterIntoAST() {
    QString base = QStringLiteral("=NOT repo:test");
    QString hideUpdated = FilterEditor::applyQuickFilter(
        base, QStringLiteral("repo"), QStringLiteral("other"), true);
    // It should convert it to an OR inside the NOT
    QVERIFY(
        hideUpdated.contains(QStringLiteral("NOT (repo:test OR repo:other)")));
  }
};

QTEST_MAIN(TestFilter)
#include "test_filter.moc"
