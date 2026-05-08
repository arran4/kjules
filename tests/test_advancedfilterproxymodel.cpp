#include "../src/advancedfilterproxymodel.h"
#include <QStandardItemModel>
#include <QtTest>

class TestAdvancedFilterProxyModel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testFilterAcceptsRow() {
    QStandardItemModel sourceModel(3, 2);
    sourceModel.setHorizontalHeaderLabels({"Name", "Description"});

    sourceModel.setItem(0, 0, new QStandardItem("apple/repo1"));
    sourceModel.setItem(0, 1, new QStandardItem("A test repository"));

    sourceModel.setItem(1, 0, new QStandardItem("banana/repo2"));
    sourceModel.setItem(1, 1, new QStandardItem("Another repository"));

    sourceModel.setItem(2, 0, new QStandardItem("apple/repo3"));
    sourceModel.setItem(2, 1, new QStandardItem("Yet another test"));

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);

    // Default global substring search
    proxyModel.setFilterQuery("test");
    QCOMPARE(proxyModel.rowCount(), 2);

    proxyModel.setFilterQuery("Another");
    QCOMPARE(proxyModel.rowCount(), 2);

    // Empty query
    proxyModel.setFilterQuery("");
    QCOMPARE(proxyModel.rowCount(), 3);

    // AST query - relies on Name column for owner/repo parsing
    // KeyValueNode::evaluate does wildcard match for 'owner' and 'repo'
    // keywords natively.
    proxyModel.setFilterQuery("=owner:apple");
    QCOMPARE(proxyModel.rowCount(), 2);

    proxyModel.setFilterQuery("=owner:banana");
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery("=repo:repo2");
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery("=repo:repo3");
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery("=owner:apple AND repo:repo3");
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery("=description:\"Yet another test\"");
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery("=description:test");
    QCOMPARE(proxyModel.rowCount(), 2);

    // Complex query
    proxyModel.setFilterQuery("=(owner:apple AND repo:repo1) OR owner:banana");
    QCOMPARE(proxyModel.rowCount(), 2);
  }
};

QTEST_MAIN(TestAdvancedFilterProxyModel)
#include "test_advancedfilterproxymodel.moc"
