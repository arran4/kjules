#include <QtTest>
#include "../src/filterparser.h"
#include "../src/filtereditor.h"

class TestFilter : public QObject {
    Q_OBJECT

private slots:
    void testApplyQuickFilter() {
        QString base = "=repo:test";
        QString updated = FilterEditor::applyQuickFilter(base, "owner", "jules", false);
        QVERIFY(updated.contains("owner:jules"));
        QVERIFY(updated.contains("repo:test"));
        QVERIFY(updated.contains("AND"));

        QString hideUpdated = FilterEditor::applyQuickFilter(base, "owner", "jules", true);
        QVERIFY(hideUpdated.contains("NOT owner:jules"));
    }

    void testMergeFilterIntoAST() {
        QString base = "=NOT repo:test";
        QString hideUpdated = FilterEditor::applyQuickFilter(base, "repo", "other", true);
        // It should convert it to an OR inside the NOT
        QVERIFY(hideUpdated.contains("NOT (repo:test OR repo:other)"));
    }
};

QTEST_MAIN(TestFilter)
#include "test_filter.moc"
