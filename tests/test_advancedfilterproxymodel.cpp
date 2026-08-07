#include "../src/advancedfilterproxymodel.h"
#include <QStandardItemModel>
#include <QStringList>
#include <QtTest>

#include "../src/sessionmodel.h"
#include "../src/sourcemodel.h"

class MockSourceModel : public SourceModel {
public:
  MockSourceModel(QObject *parent = nullptr) : SourceModel(parent) {}
  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return m_favourites.size();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return 1;
  }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= m_favourites.size())
      return QVariant();
    if (role == SourceModel::FavouriteRole)
      return m_favourites[index.row()];
    // Return dummy data for display role so lessThan can fallback if needed
    if (role == Qt::DisplayRole)
      return QStringLiteral("Item %1").arg(index.row());
    return QVariant();
  }
  void setFavourites(const QList<QVariant> &favs) {
    beginResetModel();
    m_favourites = favs;
    endResetModel();
  }

private:
  QList<QVariant> m_favourites;
};

class MockSessionModel : public SessionModel {
public:
  MockSessionModel(QObject *parent = nullptr) : SessionModel(QStringLiteral(""), parent) {}
  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return m_favourites.size();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return SessionModel::ColId + 1; // Assuming ColId is the last one in the enum
  }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= m_favourites.size())
      return QVariant();

    if (role == SessionModel::FavouriteRole)
      return m_favourites[index.row()];

    if (index.column() == SessionModel::ColPRLabels && role == Qt::DisplayRole) {
      if (index.row() == 0)
        return QStringLiteral("bug, urgent");
      if (index.row() == 1)
        return QStringLiteral("enhancement");
    }

    if (index.column() == SessionModel::ColTitle && role == Qt::DisplayRole) {
      return QStringLiteral("Session %1").arg(index.row() + 1);
    }

    if (role == Qt::DisplayRole)
      return QStringLiteral("Item %1").arg(index.row());

    return QVariant();
  }
  void setFavourites(const QList<QVariant> &favs) {
    beginResetModel();
    m_favourites = favs;
    endResetModel();
  }

private:
  QList<QVariant> m_favourites;
};

class TestAdvancedFilterProxyModel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testFilterAcceptsRow() {
    QStandardItemModel sourceModel(3, 2);
    sourceModel.setHorizontalHeaderLabels(QStringList() << QStringLiteral("Name") << QStringLiteral("Description"));

    sourceModel.setItem(0, 0, new QStandardItem(QStringLiteral("apple/repo1")));
    sourceModel.setItem(0, 1, new QStandardItem(QStringLiteral("A test repository")));

    sourceModel.setItem(1, 0, new QStandardItem(QStringLiteral("banana/repo2")));
    sourceModel.setItem(1, 1, new QStandardItem(QStringLiteral("Another repository")));

    sourceModel.setItem(2, 0, new QStandardItem(QStringLiteral("apple/repo3")));
    sourceModel.setItem(2, 1, new QStandardItem(QStringLiteral("Yet another test")));

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);
    QCOMPARE(proxyModel.columnCount(), 2);

    // Default global substring search
    proxyModel.setFilterQuery(QStringLiteral("test"));
    QCOMPARE(proxyModel.rowCount(), 2);
    QCOMPARE(proxyModel.columnCount(), 2);

    proxyModel.setFilterQuery(QStringLiteral("Another"));
    QCOMPARE(proxyModel.rowCount(), 2);

    // Empty query
    proxyModel.setFilterQuery(QStringLiteral(""));
    QCOMPARE(proxyModel.rowCount(), 3);
    QCOMPARE(proxyModel.columnCount(), 2);

    // Repeated transitions between text and empty queries must preserve the
    // proxy's source rows and columns. NewSessionDialog stacks its branch list
    // behind this proxy and exercises this path on every keystroke.
    proxyModel.setFilterQuery(QStringLiteral("missing"));
    QCOMPARE(proxyModel.rowCount(), 0);
    QCOMPARE(proxyModel.columnCount(), 2);
    proxyModel.setFilterQuery(QStringLiteral(""));
    QCOMPARE(proxyModel.rowCount(), 3);
    QCOMPARE(proxyModel.columnCount(), 2);

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

    proxyModel.setFilterQuery(QStringLiteral("=description:\"Yet another test\""));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("=description:test"));
    QCOMPARE(proxyModel.rowCount(), 2);

    // Complex query
    proxyModel.setFilterQuery(QStringLiteral("=(owner:apple AND repo:repo1) OR owner:banana"));
    QCOMPARE(proxyModel.rowCount(), 2);
  }

  void testLessThanSourceModel() {
    MockSourceModel sourceModel;
    // Set 4 items: row 0 = fav 10, row 1 = fav 5, row 2 = fav 10 (equal), row 3
    // = no fav (QVariant invalid)
    sourceModel.setFavourites(QList<QVariant>() << 10 << 5 << 10 << QVariant());

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);

    proxyModel.sort(0, Qt::AscendingOrder);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(0, 0)).row(), 0);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(1, 0)).row(), 2);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(2, 0)).row(), 1);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(3, 0)).row(), 3);

    // Sort Descending: Favorites still at the top (highest rank first),
    // but tie-breakers (like string sorting) are inverted.
    proxyModel.sort(0, Qt::DescendingOrder);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(0, 0)).row(), 2);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(1, 0)).row(), 0);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(2, 0)).row(), 1);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(3, 0)).row(), 3);
  }

  void testLabelFiltering() {
    MockSessionModel sessionModel;
    sessionModel.setFavourites(QList<QVariant>() << 0 << 0); // 2 items

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sessionModel);

    proxyModel.setFilterQuery(QStringLiteral("=label:\"bug\""));
    QCOMPARE(proxyModel.rowCount(), 1);
    QModelIndex idx = proxyModel.index(0, SessionModel::ColTitle);
    QCOMPARE(proxyModel.data(idx).toString(), QStringLiteral("Session 1"));

    proxyModel.setFilterQuery(QStringLiteral("=NOT label:\"bug\""));
    QCOMPARE(proxyModel.rowCount(), 1);
    idx = proxyModel.index(0, SessionModel::ColTitle);
    QCOMPARE(proxyModel.data(idx).toString(), QStringLiteral("Session 2"));
  }

  void testLessThanSessionModel() {
    MockSessionModel sessionModel;
    sessionModel.setFavourites(QList<QVariant>() << 2 << 8 << QVariant() << 8);

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sessionModel);

    proxyModel.sort(0, Qt::AscendingOrder);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(0, 0)).row(), 1);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(1, 0)).row(), 3);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(2, 0)).row(), 0);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(3, 0)).row(), 2);

    proxyModel.sort(0, Qt::DescendingOrder);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(0, 0)).row(), 3);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(1, 0)).row(), 1);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(2, 0)).row(), 0);
    QCOMPARE(proxyModel.mapToSource(proxyModel.index(3, 0)).row(), 2);
  }
};

QTEST_MAIN(TestAdvancedFilterProxyModel)
#include "test_advancedfilterproxymodel.moc"
