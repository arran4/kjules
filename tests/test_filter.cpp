#include "../src/filtereditor.h"
#include "../src/filterparser.h"
#include <QtTest>

class MockAccessor : public FilterDataAccessor {
public:
  QMap<QString, QString> data;
  QString getValue(const QString &key) const override {
    return data.value(key);
  }
  QList<QString> getAllValues() const override { return data.values(); }
};

class TestFilter : public QObject {
  Q_OBJECT

private Q_SLOTS:

  void testInNodeEvaluate() {
    MockAccessor accessor;
    accessor.data.insert(QStringLiteral("state"), QStringLiteral("open"));
    accessor.data.insert(QStringLiteral("author"), QStringLiteral("jules"));

    // Happy path: exact match
    InNode exactNode(QStringLiteral("state"), QStringLiteral("open,closed"));
    QVERIFY(exactNode.evaluate(accessor));

    // Case insensitivity
    InNode caseNode(QStringLiteral("state"), QStringLiteral("OPEN,merged"));
    QVERIFY(caseNode.evaluate(accessor));

    // No match
    InNode noMatchNode(QStringLiteral("state"),
                       QStringLiteral("closed,merged"));
    QVERIFY(!noMatchNode.evaluate(accessor));

    // Missing key
    InNode missingKeyNode(QStringLiteral("missing"),
                          QStringLiteral("open,closed"));
    QVERIFY(!missingKeyNode.evaluate(accessor));

    // Whitespace handling (should match because of trimming)
    InNode whitespaceNode(QStringLiteral("author"),
                          QStringLiteral(" alice , jules , bob "));
    QVERIFY(whitespaceNode.evaluate(accessor));

    // Test toString serialization
    QVERIFY(exactNode.toString() == QStringLiteral("state IN \"open,closed\""));

    // Verify matching against non-first elements
    InNode secondMatchNode(QStringLiteral("state"),
                           QStringLiteral("closed,open"));
    QVERIFY(secondMatchNode.evaluate(accessor));

    // Edge case: whitespace-only values in the list should not match missing
    // keys
    InNode emptyValueNode(QStringLiteral("missing"), QStringLiteral(" , "));
    QVERIFY(!emptyValueNode.evaluate(accessor));
  }

  void testKeyValueNodeDateFilterEvaluate() {
    MockAccessor accessor;
    accessor.data.insert(QStringLiteral("createdat"),
                         QStringLiteral("2023-01-15T12:00:00Z"));
    accessor.data.insert(QStringLiteral("updatedat"),
                         QStringLiteral("2023-02-15T12:00:00Z"));

    // Happy path: created-before
    KeyValueNode createdBeforeNode(QStringLiteral("created-before"),
                                   QStringLiteral("2023-01-20T12:00:00Z"));
    QVERIFY(createdBeforeNode.evaluate(accessor));

    // created-before fails
    KeyValueNode createdBeforeNodeFail(QStringLiteral("created-before"),
                                       QStringLiteral("2023-01-10T12:00:00Z"));
    QVERIFY(!createdBeforeNodeFail.evaluate(accessor));

    // Happy path: created-after
    KeyValueNode createdAfterNode(QStringLiteral("created-after"),
                                  QStringLiteral("2023-01-10T12:00:00Z"));
    QVERIFY(createdAfterNode.evaluate(accessor));

    // created-after fails
    KeyValueNode createdAfterNodeFail(QStringLiteral("created-after"),
                                      QStringLiteral("2023-01-20T12:00:00Z"));
    QVERIFY(!createdAfterNodeFail.evaluate(accessor));

    // Happy path: updated-before
    KeyValueNode updatedBeforeNode(QStringLiteral("updated-before"),
                                   QStringLiteral("2023-02-20T12:00:00Z"));
    QVERIFY(updatedBeforeNode.evaluate(accessor));

    // Happy path: updated-after
    KeyValueNode updatedAfterNode(QStringLiteral("updated-after"),
                                  QStringLiteral("2023-02-10T12:00:00Z"));
    QVERIFY(updatedAfterNode.evaluate(accessor));

    // Exact same date should return false
    KeyValueNode exactDateNode(QStringLiteral("created-before"),
                               QStringLiteral("2023-01-15T12:00:00Z"));
    QVERIFY(!exactDateNode.evaluate(accessor));

    // Invalid date in filter
    KeyValueNode invalidFilterDateNode(QStringLiteral("created-before"),
                                       QStringLiteral("not-a-date"));
    QVERIFY(!invalidFilterDateNode.evaluate(accessor));

    // Invalid date in data
    MockAccessor invalidDataAccessor;
    invalidDataAccessor.data.insert(QStringLiteral("createdat"),
                                    QStringLiteral("invalid-date"));
    KeyValueNode validFilterInvalidDataNode(
        QStringLiteral("created-before"),
        QStringLiteral("2023-01-20T12:00:00Z"));
    QVERIFY(!validFilterInvalidDataNode.evaluate(invalidDataAccessor));
  }

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
