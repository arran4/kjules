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

    // No Fav vs No Fav (fallback to default string comparison)
    // Create two items with no favourite to test fallback
    SourceModel sourceModelFallback;
    QJsonObject srcFallback1;
    srcFallback1[QStringLiteral("id")] = QStringLiteral("sourceB");
    srcFallback1[QStringLiteral("name")] = QStringLiteral("B Source");
    QJsonObject srcFallback2;
    srcFallback2[QStringLiteral("id")] = QStringLiteral("sourceA");
    srcFallback2[QStringLiteral("name")] = QStringLiteral("A Source");

    QJsonArray fallbackSources;
    fallbackSources.append(srcFallback1);
    fallbackSources.append(srcFallback2);
    sourceModelFallback.setSources(fallbackSources);

    TestableProxyModel proxyModelFallback;
    proxyModelFallback.setSourceModel(&sourceModelFallback);

    QModelIndex idxFB1 =
        sourceModelFallback.index(0, SourceModel::ColName); // "B Source"
    QModelIndex idxFB2 =
        sourceModelFallback.index(1, SourceModel::ColName); // "A Source"

    proxyModelFallback.sort(SourceModel::ColName, Qt::AscendingOrder);
    // "B Source" < "A Source" is false
    QVERIFY(proxyModelFallback.lessThan(idxFB1, idxFB2) == false);
    // "A Source" < "B Source" is true
    QVERIFY(proxyModelFallback.lessThan(idxFB2, idxFB1) == true);

    proxyModelFallback.sort(SourceModel::ColName, Qt::DescendingOrder);
    // QSortFilterProxyModel::lessThan returns true if left < right regardless
    // of sort order
    QVERIFY(proxyModelFallback.lessThan(idxFB1, idxFB2) == false);
    QVERIFY(proxyModelFallback.lessThan(idxFB2, idxFB1) == true);
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

    // No Fav vs No Fav (fallback to default string comparison)
    SessionModel sessionModelFallback;
    QJsonObject sessFallback1;
    sessFallback1[QStringLiteral("id")] = QStringLiteral("sessionB");
    sessFallback1[QStringLiteral("title")] = QStringLiteral("B Session");
    QJsonObject sessFallback2;
    sessFallback2[QStringLiteral("id")] = QStringLiteral("sessionA");
    sessFallback2[QStringLiteral("title")] = QStringLiteral("A Session");

    QJsonArray fallbackSessions;
    fallbackSessions.append(sessFallback1);
    fallbackSessions.append(sessFallback2);
    sessionModelFallback.setSessions(fallbackSessions);

    TestableProxyModel proxyModelFallback;
    proxyModelFallback.setSourceModel(&sessionModelFallback);

    QModelIndex idxFB1 =
        sessionModelFallback.index(0, SessionModel::ColTitle); // "B Session"
    QModelIndex idxFB2 =
        sessionModelFallback.index(1, SessionModel::ColTitle); // "A Session"

    proxyModelFallback.sort(SessionModel::ColTitle, Qt::AscendingOrder);
    QVERIFY(proxyModelFallback.lessThan(idxFB1, idxFB2) == false);
    QVERIFY(proxyModelFallback.lessThan(idxFB2, idxFB1) == true);

    proxyModelFallback.sort(SessionModel::ColTitle, Qt::DescendingOrder);
    QVERIFY(proxyModelFallback.lessThan(idxFB1, idxFB2) == false);
    QVERIFY(proxyModelFallback.lessThan(idxFB2, idxFB1) == true);
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
