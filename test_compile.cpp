#include <QCoreApplication>
#include <QStandardItemModel>
#include "src/advancedfilterproxymodel.h"
#include <QDebug>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

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

    proxyModel.setFilterQuery("test");
    qDebug() << "test count:" << proxyModel.rowCount();

    proxyModel.setFilterQuery("=owner:apple");
    qDebug() << "=owner:apple count:" << proxyModel.rowCount();

    return 0;
}
