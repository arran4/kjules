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
