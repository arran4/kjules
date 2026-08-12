#include "../src/advancedfilterproxymodel.h"
#include <QStandardItemModel>
#include <QStringList>
#include <QtTest>

#include "../src/sessionmodel.h"
#include "../src/sourcemodel.h"

class MockSourceModel : public SourceModel {
public:
  MockSourceModel(QObject *parent = nullptr) : SourceModel(parent, StorageMode::InMemory) {}
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

  void testSourceModelNameRoleFallback() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    QJsonArray sources;
    QJsonObject s1;
    s1[QStringLiteral("id")] = QStringLiteral("sources/github/kde/kjules");
    sources.append(s1);
    model.setSources(sources);

    QCOMPARE(model.rowCount(), 1);
    QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, SourceModel::NameRole).toString(), QStringLiteral("kde/kjules"));
    QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QStringLiteral("kde/kjules"));
  }

  void testSourceModelPreservesUnmatchedCustomSources() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    model.setSources({});
    model.addSources({QJsonObject{{QStringLiteral("id"), QStringLiteral("manual/repository")},
                                  {QStringLiteral("name"), QStringLiteral("manual/repository")},
                                  {QStringLiteral("isCustom"), true}}});

    model.setSources({QJsonObject{{QStringLiteral("id"), QStringLiteral("sources/github/kde/kjules")},
                                  {QStringLiteral("name"), QStringLiteral("sources/github/kde/kjules")}}});

    QCOMPARE(model.rowCount(), 2);
    bool foundCustom = false;
    for (int row = 0; row < model.rowCount(); ++row) {
      const QJsonObject raw = model.data(model.index(row, 0), SourceModel::RawDataRole).toJsonObject();
      if (raw.value(QStringLiteral("id")).toString() == QStringLiteral("manual/repository")) {
        foundCustom = raw.value(QStringLiteral("isCustom")).toBool();
      }
    }
    QVERIFY(foundCustom);
  }

  void testSourceModelMigratesCustomSourceToApiSource() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    model.setSources({});
    model.addSources({QJsonObject{{QStringLiteral("id"), QStringLiteral("arran4/blog")},
                                  {QStringLiteral("name"), QStringLiteral("arran4/blog")},
                                  {QStringLiteral("isCustom"), true}}});

    model.setSources({QJsonObject{{QStringLiteral("name"), QStringLiteral("sources/github/kde/kjules")}}});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.addSources({QJsonObject{{QStringLiteral("name"), QStringLiteral("sources/github/arran4/blog")}}}),
             0);

    QCOMPARE(model.rowCount(), 2);
    bool foundMigrated = false;
    for (int row = 0; row < model.rowCount(); ++row) {
      const QModelIndex index = model.index(row, 0);
      if (model.data(index, SourceModel::IdRole).toString() == QStringLiteral("sources/github/arran4/blog")) {
        foundMigrated =
            !model.data(index, SourceModel::RawDataRole).toJsonObject().value(QStringLiteral("isCustom")).toBool();
      }
    }
    QVERIFY(foundMigrated);
  }

  void testSourceModelKeepsDistinctOpaqueResourceNames() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    model.setSources({QJsonObject{{QStringLiteral("name"), QStringLiteral("sources/acme/repository")}},
                      QJsonObject{{QStringLiteral("name"), QStringLiteral("sources/github/acme/repository")},
                                  {QStringLiteral("githubRepo"),
                                   QJsonObject{{QStringLiteral("owner"), QStringLiteral("acme")},
                                               {QStringLiteral("repo"), QStringLiteral("repository")}}}}});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), SourceModel::IdRole).toString(), QStringLiteral("sources/acme/repository"));
    QCOMPARE(model.data(model.index(1, 0), SourceModel::IdRole).toString(),
             QStringLiteral("sources/github/acme/repository"));

    model.recordSessionCreated(QStringLiteral("sources/acme/repository"));
    QCOMPARE(model.data(model.index(0, SourceModel::ColManagedSessions), Qt::DisplayRole).toInt(), 1);
    QCOMPARE(model.data(model.index(1, SourceModel::ColManagedSessions), Qt::DisplayRole).toInt(), 0);
  }

  void testSourceModelUsesDocumentedNameAndGithubMetadata() {
    const QJsonObject source{
        {QStringLiteral("name"), QStringLiteral("sources/opaque-value")},
        {QStringLiteral("id"), QStringLiteral("output-only-id")},
        {QStringLiteral("githubRepo"), QJsonObject{{QStringLiteral("owner"), QStringLiteral("example")},
                                                   {QStringLiteral("repo"), QStringLiteral("project")}}}};
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    model.setSources({source});

    QCOMPARE(model.data(model.index(0, 0), SourceModel::IdRole).toString(), QStringLiteral("sources/opaque-value"));
    QCOMPARE(SourceModel::githubOwner(source), QStringLiteral("example"));
    QCOMPARE(SourceModel::githubRepository(source), QStringLiteral("project"));
    QCOMPARE(SourceModel::repositoryUrl(source), QStringLiteral("https://github.com/example/project"));
  }

  void testSourceSnapshotRemovesStaleApiNamesButKeepsCustomSources() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    model.setSources({QJsonObject{{QStringLiteral("name"), QStringLiteral("sources/arran4/arrans_overlay")}}});
    model.addSources({QJsonObject{{QStringLiteral("name"), QStringLiteral("custom/repository")},
                                  {QStringLiteral("isCustom"), true}}});

    model.setSources({QJsonObject{
        {QStringLiteral("name"), QStringLiteral("sources/github/arran4/arrans_overlay")},
        {QStringLiteral("githubRepo"), QJsonObject{{QStringLiteral("owner"), QStringLiteral("arran4")},
                                                   {QStringLiteral("repo"), QStringLiteral("arrans_overlay")}}}}});

    QCOMPARE(model.rowCount(), 2);
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
      names.append(model.data(model.index(row, 0), SourceModel::IdRole).toString());
    }
    QVERIFY(!names.contains(QStringLiteral("sources/arran4/arrans_overlay")));
    QVERIFY(names.contains(QStringLiteral("sources/github/arran4/arrans_overlay")));
    QVERIFY(names.contains(QStringLiteral("custom/repository")));
  }

  void testEmptyEqualsFormulaFilter() {
    SourceModel model(nullptr, SourceModel::StorageMode::InMemory);
    QJsonArray sources;
    QJsonObject s1;
    s1[QStringLiteral("id")] = QStringLiteral("sources/github/kde/kjules");
    sources.append(s1);
    model.setSources(sources);

    AdvancedFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&model);

    proxyModel.setFilterQuery(QStringLiteral("="));
    QCOMPARE(proxyModel.rowCount(), 1);

    proxyModel.setFilterQuery(QStringLiteral("= "));
    QCOMPARE(proxyModel.rowCount(), 1);
  }

  void testSessionModelUndefinedPrUrl() {
    SessionModel model(QStringLiteral("test"));
    QJsonArray sessions;
    QJsonObject session;
    session[QStringLiteral("name")] = QStringLiteral("sessions/123");
    session[QStringLiteral("id")] = QStringLiteral("123");

    QJsonArray outputs;
    QJsonObject out;
    QJsonObject pr;
    pr[QStringLiteral("url")] = QStringLiteral("undefined");
    out[QStringLiteral("pullRequest")] = pr;
    outputs.append(out);
    session[QStringLiteral("outputs")] = outputs;

    sessions.append(session);
    model.setSessions(sessions);

    QCOMPARE(model.rowCount(), 1);
    QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, SessionModel::PrUrlRole).toString(), QString());
  }
};

QTEST_MAIN(TestAdvancedFilterProxyModel)
#include "test_advancedfilterproxymodel.moc"
