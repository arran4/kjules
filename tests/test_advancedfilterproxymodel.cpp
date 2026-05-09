#include "../src/advancedfilterproxymodel.h"
#include <QStandardItemModel>
#include <QStringList>
#include <QtTest>

class TestAdvancedFilterProxyModel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testFilterAcceptsRow() {
    QStandardItemModel sourceModel(3, 2);
    sourceModel.setHorizontalHeaderLabels(QStringList()
                                          << QStringLiteral("Name")
                                          << QStringLiteral("Description"));

    sourceModel.setItem(0, 0, new QStandardItem(QStringLiteral("apple/repo1")));
    sourceModel.setItem(0, 1,
                        new QStandardItem(QStringLiteral("A test repository")));

    sourceModel.setItem(1, 0,
                        new QStandardItem(QStringLiteral("banana/repo2")));
    sourceModel.setItem(
        1, 1, new QStandardItem(QStringLiteral("Another repository")));

    sourceModel.setItem(2, 0, new QStandardItem(QStringLiteral("apple/repo3")));
    sourceModel.setItem(2, 1,
                        new QStandardItem(QStringLiteral("Yet another test")));

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);

    // Default global substring search
    proxyModel.setFilterQuery(QStringLiteral("test"));
    QCOMPARE(proxyModel.rowCount(), 2);

    proxyModel.setFilterQuery(QStringLiteral("Another"));
    QCOMPARE(proxyModel.rowCount(), 2);

    // Empty query
    proxyModel.setFilterQuery(QStringLiteral(""));
    QCOMPARE(proxyModel.rowCount(), 3);

    // AST query - relies on Name column for owner/repo parsing
    // KeyValueNode::evaluate does wildcard match for 'owner' and 'repo'
    // keywords natively.
    proxyModel.setFilterQuery(QStringLiteral("=owner:apple"));
    QCOMPARE(proxyModel.rowCount(), 2);

    proxyModel.setFilterQuery(QStringLiteral("=owner:banana"));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("=repo:repo2"));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("=repo:repo3"));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("=owner:apple AND repo:repo3"));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(
        QStringLiteral("=description:\"Yet another test\""));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("=description:test"));
    QCOMPARE(proxyModel.rowCount(), 2);

    // Complex query
    proxyModel.setFilterQuery(
        QStringLiteral("=(owner:apple AND repo:repo1) OR owner:banana"));
    QCOMPARE(proxyModel.rowCount(), 2);
  }
};

QTEST_MAIN(TestAdvancedFilterProxyModel)
#include "test_advancedfilterproxymodel.moc"
