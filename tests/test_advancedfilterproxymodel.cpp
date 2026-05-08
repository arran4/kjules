#include "../src/advancedfilterproxymodel.h"
#include "../src/sessionmodel.h"
#include "../src/sourcemodel.h"
#include <QStandardItemModel>
#include <QtTest>

class TestableProxyModel : public AdvancedFilterProxyModel {
public:
  using AdvancedFilterProxyModel::lessThan;
};

class TestAdvancedFilterProxyModel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testLessThanSourceModel() {
    SourceModel sourceModel;

    QJsonObject src1;
    src1[QStringLiteral("id")] = QStringLiteral("source1");
    src1[QStringLiteral("name")] = QStringLiteral("Source 1");
    src1[QStringLiteral("local_favourite")] = 1; // Fav rank 1

    QJsonObject src2;
    src2[QStringLiteral("id")] = QStringLiteral("source2");
    src2[QStringLiteral("name")] = QStringLiteral("Source 2");
    // source2 has no favourite

    QJsonObject src3;
    src3[QStringLiteral("id")] = QStringLiteral("source3");
    src3[QStringLiteral("name")] = QStringLiteral("Source 3");
    src3[QStringLiteral("local_favourite")] = 2; // Fav rank 2

    QJsonArray sources;
    sources.append(src1);
    sources.append(src2);
    sources.append(src3);

    sourceModel.setSources(sources);

    TestableProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);

    QModelIndex idx1 =
        sourceModel.index(0, SourceModel::ColName); // src1 (Fav 1)
    QModelIndex idx2 =
        sourceModel.index(1, SourceModel::ColName); // src2 (No fav)
    QModelIndex idx3 =
        sourceModel.index(2, SourceModel::ColName); // src3 (Fav 2)

    // Ascending Order
    proxyModel.sort(SourceModel::ColName, Qt::AscendingOrder);

    // Fav 2 vs Fav 1 (2 > 1 => returns true in ascending order, so Fav 2 goes
    // before Fav 1)
    QVERIFY(proxyModel.lessThan(idx3, idx1) == true);
    QVERIFY(proxyModel.lessThan(idx1, idx3) == false);

    // Fav 1 vs No Fav (1 > -1 => returns true in ascending order)
    QVERIFY(proxyModel.lessThan(idx1, idx2) == true);
    QVERIFY(proxyModel.lessThan(idx2, idx1) == false);

    // Descending Order
    proxyModel.sort(SourceModel::ColName, Qt::DescendingOrder);

    // Fav 2 vs Fav 1 (2 > 1 => returns false in descending order, leftFav <
    // rightFav is false)
    QVERIFY(proxyModel.lessThan(idx3, idx1) == false);
    QVERIFY(proxyModel.lessThan(idx1, idx3) == true);

    // Fav 1 vs No Fav (1 > -1 => returns false in descending order)
    QVERIFY(proxyModel.lessThan(idx1, idx2) == false);
    QVERIFY(proxyModel.lessThan(idx2, idx1) == true);
  }

  void testLessThanSessionModel() {
    SessionModel sessionModel;

    QJsonObject sess1;
    sess1[QStringLiteral("id")] = QStringLiteral("session1");
    sess1[QStringLiteral("title")] = QStringLiteral("Session 1");
    sess1[QStringLiteral("local_favourite")] = 1;

    QJsonObject sess2;
    sess2[QStringLiteral("id")] = QStringLiteral("session2");
    sess2[QStringLiteral("title")] = QStringLiteral("Session 2");

    QJsonObject sess3;
    sess3[QStringLiteral("id")] = QStringLiteral("session3");
    sess3[QStringLiteral("title")] = QStringLiteral("Session 3");
    sess3[QStringLiteral("local_favourite")] = 3;

    QJsonArray sessions;
    sessions.append(sess1);
    sessions.append(sess2);
    sessions.append(sess3);

    sessionModel.setSessions(sessions);

    TestableProxyModel proxyModel;
    proxyModel.setSourceModel(&sessionModel);

    QModelIndex idx1 =
        sessionModel.index(0, SessionModel::ColTitle); // sess1 (Fav 1)
    QModelIndex idx2 =
        sessionModel.index(1, SessionModel::ColTitle); // sess2 (No fav)
    QModelIndex idx3 =
        sessionModel.index(2, SessionModel::ColTitle); // sess3 (Fav 3)

    proxyModel.sort(SessionModel::ColTitle, Qt::AscendingOrder);

    // Fav 3 vs Fav 1 (3 > 1 => true)
    QVERIFY(proxyModel.lessThan(idx3, idx1) == true);
    // Fav 1 vs Fav 3 (1 > 3 => false)
    QVERIFY(proxyModel.lessThan(idx1, idx3) == false);

    // Fav 1 vs No Fav (1 > -1 => true)
    QVERIFY(proxyModel.lessThan(idx1, idx2) == true);

    proxyModel.sort(SessionModel::ColTitle, Qt::DescendingOrder);

    // Fav 3 vs Fav 1 (3 < 1 => false)
    QVERIFY(proxyModel.lessThan(idx3, idx1) == false);
    // Fav 1 vs Fav 3 (1 < 3 => true)
    QVERIFY(proxyModel.lessThan(idx1, idx3) == true);

    // Fav 1 vs No Fav (1 < -1 => false)
    QVERIFY(proxyModel.lessThan(idx1, idx2) == false);
  }

  void testLessThanFallbackModel() {
    QStandardItemModel standardModel(2, 1);
    standardModel.setItem(0, 0, new QStandardItem(QStringLiteral("A")));
    standardModel.setItem(1, 0, new QStandardItem(QStringLiteral("B")));

    TestableProxyModel proxyModel;
    proxyModel.setSourceModel(&standardModel);

    QModelIndex idx1 = standardModel.index(0, 0); // QStringLiteral("A")
    QModelIndex idx2 = standardModel.index(1, 0); // QStringLiteral("B")

    proxyModel.sort(0, Qt::AscendingOrder);

    // QStandardItemModel should fallback to QSortFilterProxyModel::lessThan
    QVERIFY(proxyModel.lessThan(idx1, idx2) == true);
    QVERIFY(proxyModel.lessThan(idx2, idx1) == false);

    proxyModel.sort(0, Qt::DescendingOrder);

    QVERIFY(proxyModel.lessThan(idx1, idx2) == true);
  }
};

QTEST_MAIN(TestAdvancedFilterProxyModel)
#include "test_advancedfilterproxymodel.moc"
