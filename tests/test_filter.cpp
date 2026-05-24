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

  void testNotNodeEvaluate() {
    MockAccessor accessor;
    accessor.data.insert(QStringLiteral("state"), QStringLiteral("open"));

    // Happy path: child evaluates to true, NotNode evaluates to false
    QSharedPointer<ASTNode> childTrue = QSharedPointer<InNode>::create(
        QStringLiteral("state"), QStringLiteral("open"));
    NotNode notNodeTrue(childTrue);
    QVERIFY(!notNodeTrue.evaluate(accessor));

    // Child evaluates to false, NotNode evaluates to true
    QSharedPointer<ASTNode> childFalse = QSharedPointer<InNode>::create(
        QStringLiteral("state"), QStringLiteral("closed"));
    NotNode notNodeFalse(childFalse);
    QVERIFY(notNodeFalse.evaluate(accessor));

    // Test toString serialization
    QCOMPARE(notNodeTrue.toString(), QStringLiteral("NOT state IN \"open\""));

    // Test nested NOT nodes
    QSharedPointer<ASTNode> doubleNot = QSharedPointer<NotNode>::create(
        QSharedPointer<NotNode>::create(childTrue));
    QVERIFY(doubleNot->evaluate(accessor));
    QCOMPARE(doubleNot->toString(),
             QStringLiteral("NOT NOT state IN \"open\""));
  }

  void testOrNodeEvaluate() {
    MockAccessor accessor;
    accessor.data.insert(QStringLiteral("state"), QStringLiteral("open"));
    accessor.data.insert(QStringLiteral("author"), QStringLiteral("jules"));

    auto trueNode1 = QSharedPointer<KeyValueNode>::create(QStringLiteral("state"), QStringLiteral("open"));
    auto trueNode2 = QSharedPointer<KeyValueNode>::create(QStringLiteral("author"), QStringLiteral("jules"));
    auto falseNode1 = QSharedPointer<KeyValueNode>::create(QStringLiteral("state"), QStringLiteral("closed"));
    auto falseNode2 = QSharedPointer<KeyValueNode>::create(QStringLiteral("author"), QStringLiteral("alice"));

    // Empty OrNode
    OrNode emptyNode({});
    QVERIFY(!emptyNode.evaluate(accessor));

    // Single true child
    OrNode singleTrueNode({trueNode1});
    QVERIFY(singleTrueNode.evaluate(accessor));

    // Single false child
    OrNode singleFalseNode({falseNode1});
    QVERIFY(!singleFalseNode.evaluate(accessor));

    // Multiple true children
    OrNode multipleTrueNode({trueNode1, trueNode2});
    QVERIFY(multipleTrueNode.evaluate(accessor));

    // Mixed true/false children
    OrNode mixedNode({falseNode1, trueNode1});
    QVERIFY(mixedNode.evaluate(accessor));

    OrNode mixedNode2({trueNode1, falseNode1});
    QVERIFY(mixedNode2.evaluate(accessor));

    // Multiple false children
    OrNode multipleFalseNode({falseNode1, falseNode2});
    QVERIFY(!multipleFalseNode.evaluate(accessor));

    // Test toString serialization
    QCOMPARE(multipleTrueNode.toString(),
             QStringLiteral("(state:open OR author:jules)"));
  }

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

    // Timezone handling: filter with offset (14:00+01:00 is 13:00Z, which is
    // after 12:00Z)
    KeyValueNode offsetNode(QStringLiteral("created-before"),
                            QStringLiteral("2023-01-15T14:00:00+01:00"));
    QVERIFY(offsetNode.evaluate(accessor));

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

    // Exact same date for after filter should also return false
    KeyValueNode exactDateAfterNode(QStringLiteral("created-after"),
                                    QStringLiteral("2023-01-15T12:00:00Z"));
    QVERIFY(!exactDateAfterNode.evaluate(accessor));

    // updated-before fails
    KeyValueNode updatedBeforeNodeFail(QStringLiteral("updated-before"),
                                       QStringLiteral("2023-02-10T12:00:00Z"));
    QVERIFY(!updatedBeforeNodeFail.evaluate(accessor));

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

    // Missing key in data
    MockAccessor emptyAccessor;
    QVERIFY(!createdBeforeNode.evaluate(emptyAccessor));
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
