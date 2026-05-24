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

  void testOrNodeEvaluate() {
    MockAccessor accessor;
    accessor.data.insert(QStringLiteral("state"), QStringLiteral("open"));
    accessor.data.insert(QStringLiteral("author"), QStringLiteral("jules"));

    QSharedPointer<ASTNode> trueNode1 = QSharedPointer<ASTNode>(
        new KeyValueNode(QStringLiteral("state"), QStringLiteral("open")));
    QSharedPointer<ASTNode> trueNode2 = QSharedPointer<ASTNode>(
        new KeyValueNode(QStringLiteral("author"), QStringLiteral("jules")));
    QSharedPointer<ASTNode> falseNode1 = QSharedPointer<ASTNode>(
        new KeyValueNode(QStringLiteral("state"), QStringLiteral("closed")));
    QSharedPointer<ASTNode> falseNode2 = QSharedPointer<ASTNode>(
        new KeyValueNode(QStringLiteral("author"), QStringLiteral("alice")));

    // Empty OrNode
    OrNode emptyNode({});
    QVERIFY(emptyNode.evaluate(accessor)); // based on current behavior

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
