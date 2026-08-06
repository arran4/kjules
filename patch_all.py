import os
import re

# 1. Modify sourcemodel.h
with open('src/sourcemodel.h', 'r') as f:
    h = f.read()

h = h.replace('  void decreaseFavouriteRank(const QString &id);\n  void clear();',
              '  void decreaseFavouriteRank(const QString &id);\n  void setDefaultBranches(const QString &id, const QStringList &branches);\n  static QString extractApiDefaultBranch(const QJsonObject &rawData);\n  void clear();')
with open('src/sourcemodel.h', 'w') as f:
    f.write(h)

# 2. Modify sourcemodel.cpp
with open('src/sourcemodel.cpp', 'r') as f:
    cpp = f.read()

old_update = """      if (existing.contains(QStringLiteral("local_sessionTimestamps")))
        source[QStringLiteral("local_sessionTimestamps")] = existing[QStringLiteral("local_sessionTimestamps")];
      if (existing.contains(QStringLiteral("local_favourite")))
        source[QStringLiteral("local_favourite")] = existing[QStringLiteral("local_favourite")];
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);"""

new_update = """      if (existing.contains(QStringLiteral("local_sessionTimestamps")))
        source[QStringLiteral("local_sessionTimestamps")] = existing[QStringLiteral("local_sessionTimestamps")];
      if (existing.contains(QStringLiteral("local_favourite")))
        source[QStringLiteral("local_favourite")] = existing[QStringLiteral("local_favourite")];

      QStringList localDefaults;
      if (existing.contains(QStringLiteral("local_defaultBranches"))) {
        QJsonArray arr = existing.value(QStringLiteral("local_defaultBranches")).toArray();
        for (const QJsonValue &v : arr) {
          localDefaults.append(v.toString());
        }
      } else {
        QString oldApiBranch = extractApiDefaultBranch(existing);
        if (!oldApiBranch.isEmpty()) {
          localDefaults.append(oldApiBranch);
        }
      }

      QString newApiBranch = extractApiDefaultBranch(source);
      if (!newApiBranch.isEmpty() && !localDefaults.contains(newApiBranch)) {
        localDefaults.append(newApiBranch);
      }

      if (!localDefaults.isEmpty()) {
        QJsonArray arr;
        for (const QString &b : localDefaults) {
          arr.append(b);
        }
        source[QStringLiteral("local_defaultBranches")] = arr;
      }

      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);"""

cpp = cpp.replace(old_update, new_update)

cpp += """
QString SourceModel::extractApiDefaultBranch(const QJsonObject &rawData) {
  QJsonObject githubRepo = rawData.value(QStringLiteral("githubRepo")).toObject();
  if (githubRepo.contains(QStringLiteral("defaultBranch"))) {
    QJsonObject db = githubRepo.value(QStringLiteral("defaultBranch")).toObject();
    if (db.contains(QStringLiteral("displayName"))) {
      return db.value(QStringLiteral("displayName")).toString();
    }
  }

  if (rawData.contains(QStringLiteral("defaultBranch"))) {
    return rawData.value(QStringLiteral("defaultBranch")).toString();
  }
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  if (github.contains(QStringLiteral("default_branch"))) {
    return github.value(QStringLiteral("default_branch")).toString();
  }
  return QString();
}

void SourceModel::setDefaultBranches(const QString &id, const QStringList &branches) {
  QString normId = normalizeSourceId(id);
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject obj = m_sources[i].toObject();
    QString currentId = obj.value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId = obj.value(QStringLiteral("name")).toString();
    }
    if (normalizeSourceId(currentId) == normId) {
      QJsonArray arr;
      for (const QString &b : branches) {
        arr.append(b);
      }
      obj[QStringLiteral("local_defaultBranches")] = arr;
      m_sources[i] = obj;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}
"""
with open('src/sourcemodel.cpp', 'w') as f:
    f.write(cpp)

# 3. Modify newsessiondialog.h
with open('src/newsessiondialog.h', 'r') as f:
    h = f.read()

h = h.replace('class SourceSelectionProxyModel;', 'class SourceFilterProxyModel;\nclass BranchListProxyModel;')
h = h.replace('  SourceSelectionProxyModel *m_unselectedProxy;\n  SourceSelectionProxyModel *m_selectedProxy;',
              '  SourceFilterProxyModel *m_unselectedFilterModel;\n  SourceFilterProxyModel *m_selectedFilterModel;\n  BranchListProxyModel *m_unselectedProxy;\n  BranchListProxyModel *m_selectedProxy;')

old_methods = """  void updateModels();
  QString getDefaultBranch(const QModelIndex &sourceIdx);
  QStringList getAvailableBranches(const QModelIndex &sourceIdx);
  void applyFilter();

protected:"""

new_methods = """  void updateModels();
  QStringList getAvailableBranches(const QModelIndex &sourceIdx);
  void applyFilter();

public:
  QStringList getDefaultBranches(const QModelIndex &sourceIdx);

protected:"""
h = h.replace(old_methods, new_methods)

with open('src/newsessiondialog.h', 'w') as f:
    f.write(h)

# 4. Modify newsessiondialog.cpp
with open('src/newsessiondialog.cpp', 'r') as f:
    cpp = f.read()

old_proxy_class = """class SourceSelectionProxyModel : public AdvancedFilterProxyModel {
public:
  SourceSelectionProxyModel(const QMultiMap<QString, QString> *selectedSources, bool showSelected,
                            QObject *parent = nullptr)
      : AdvancedFilterProxyModel(parent), m_selectedSources(selectedSources), m_showSelected(showSelected) {}

  void updateSelection() { invalidate(); }

  void setFilterAST(QSharedPointer<ASTNode> ast) {
    m_ast = ast;
    invalidate();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (m_showSelected && role == Qt::DisplayRole) {
      QModelIndex sourceIdx = mapToSource(index);
      QString name = sourceModel()->data(sourceIdx, SourceModel::NameRole).toString();
      if (m_selectedSources->contains(name)) {
        QStringList branches = m_selectedSources->values(name);
        branches.sort();
        QString displayName = sourceModel()->data(sourceIdx.siblingAtColumn(0), Qt::DisplayRole).toString();
        return displayName + QStringLiteral(" (") + branches.join(QStringLiteral(", ")) + QStringLiteral(")");
      }
    }
    return AdvancedFilterProxyModel::data(index, role);
  }

protected:
  bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
    bool leftFav = sourceModel()->data(source_left, SourceModel::FavouriteRole).toBool();
    bool rightFav = sourceModel()->data(source_right, SourceModel::FavouriteRole).toBool();

    if (leftFav != rightFav) {
      if (sortOrder() == Qt::AscendingOrder) {
        return leftFav;
      } else {
        return !leftFav;
      }
    }
    return AdvancedFilterProxyModel::lessThan(source_left, source_right);
  }

  class ProxyFilterDataAccessor : public FilterDataAccessor {
  public:
    ProxyFilterDataAccessor(const QModelIndex &index, const SourceModel *model) : m_index(index), m_model(model) {}

    QString getValue(const QString &key) const override {
      QString k = key.toLower();
      if (k == QStringLiteral("repo") || k == QStringLiteral("name")) {
        return m_model->data(m_index, SourceModel::NameRole).toString();
      } else if (k == QStringLiteral("owner")) {
        QString id = m_model->data(m_index, SourceModel::IdRole).toString();
        QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() >= 3 && parts[0] == QStringLiteral("sources")) {
          return parts[2];
        }
      } else if (k == QStringLiteral("title")) {
        return m_model->data(m_index.siblingAtColumn(0), Qt::DisplayRole).toString();
      }

      QJsonObject rawData = m_model->data(m_index, SourceModel::RawDataRole).toJsonObject();
      QJsonObject github = rawData.value(QStringLiteral("github")).toObject();

      if (k == QStringLiteral("archived") || k == QStringLiteral("isarchived")) {
        if (rawData.contains(QStringLiteral("isArchived"))) {
          return rawData.value(QStringLiteral("isArchived")).toBool() ? QStringLiteral("true")
                                                                      : QStringLiteral("false");
        }
        return github.value(QStringLiteral("archived")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("fork") || k == QStringLiteral("isfork")) {
        if (rawData.contains(QStringLiteral("isFork"))) {
          return rawData.value(QStringLiteral("isFork")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return github.value(QStringLiteral("fork")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("private") || k == QStringLiteral("isprivate")) {
        if (rawData.contains(QStringLiteral("isPrivate"))) {
          return rawData.value(QStringLiteral("isPrivate")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return github.value(QStringLiteral("private")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("public") || k == QStringLiteral("ispublic")) {
        if (rawData.contains(QStringLiteral("isPrivate"))) {
          return rawData.value(QStringLiteral("isPrivate")).toBool() ? QStringLiteral("false") : QStringLiteral("true");
        }
        return github.value(QStringLiteral("private")).toBool() ? QStringLiteral("false") : QStringLiteral("true");
      } else if (k == QStringLiteral("language")) {
        return github.value(QStringLiteral("language")).toString();
      }

      return QString();
    }

  private:
    QModelIndex m_index;
    const SourceModel *m_model;
  };

  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

    // First apply AST filtering if present
    if (m_ast) {
      ProxyFilterDataAccessor accessor(idx, qobject_cast<SourceModel *>(sourceModel()));
      if (!m_ast->evaluate(accessor)) {
        return false;
      }
    } else {
      if (!AdvancedFilterProxyModel::filterAcceptsRow(source_row, source_parent))
        return false;
    }

    QString name = sourceModel()->data(idx, SourceModel::NameRole).toString();
    bool isSelected = m_selectedSources->contains(name);

    if (m_showSelected) {
      return isSelected;
    }

    if (isSelected) {
      return false;
    }

    return true;
  }

private:
  const QMultiMap<QString, QString> *m_selectedSources;
  bool m_showSelected;
  QSharedPointer<ASTNode> m_ast;
};"""

new_proxy_class = """class SourceFilterProxyModel : public AdvancedFilterProxyModel {
public:
  SourceFilterProxyModel(QObject *parent = nullptr)
      : AdvancedFilterProxyModel(parent) {}

  void setFilterAST(QSharedPointer<ASTNode> ast) {
    m_ast = ast;
    invalidate();
  }

protected:
  bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
    bool leftFav = sourceModel()->data(source_left, SourceModel::FavouriteRole).toBool();
    bool rightFav = sourceModel()->data(source_right, SourceModel::FavouriteRole).toBool();

    if (leftFav != rightFav) {
      if (sortOrder() == Qt::AscendingOrder) {
        return leftFav;
      } else {
        return !leftFav;
      }
    }
    return AdvancedFilterProxyModel::lessThan(source_left, source_right);
  }

  class ProxyFilterDataAccessor : public FilterDataAccessor {
  public:
    ProxyFilterDataAccessor(const QModelIndex &index, const SourceModel *model) : m_index(index), m_model(model) {}

    QString getValue(const QString &key) const override {
      QString k = key.toLower();
      if (k == QStringLiteral("repo") || k == QStringLiteral("name")) {
        return m_model->data(m_index, SourceModel::NameRole).toString();
      } else if (k == QStringLiteral("owner")) {
        QString id = m_model->data(m_index, SourceModel::IdRole).toString();
        QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() >= 3 && parts[0] == QStringLiteral("sources")) {
          return parts[2];
        }
      } else if (k == QStringLiteral("title")) {
        return m_model->data(m_index.siblingAtColumn(0), Qt::DisplayRole).toString();
      }

      QJsonObject rawData = m_model->data(m_index, SourceModel::RawDataRole).toJsonObject();
      QJsonObject github = rawData.value(QStringLiteral("github")).toObject();

      if (k == QStringLiteral("archived") || k == QStringLiteral("isarchived")) {
        if (rawData.contains(QStringLiteral("isArchived"))) {
          return rawData.value(QStringLiteral("isArchived")).toBool() ? QStringLiteral("true")
                                                                      : QStringLiteral("false");
        }
        return github.value(QStringLiteral("archived")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("fork") || k == QStringLiteral("isfork")) {
        if (rawData.contains(QStringLiteral("isFork"))) {
          return rawData.value(QStringLiteral("isFork")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return github.value(QStringLiteral("fork")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("private") || k == QStringLiteral("isprivate")) {
        if (rawData.contains(QStringLiteral("isPrivate"))) {
          return rawData.value(QStringLiteral("isPrivate")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return github.value(QStringLiteral("private")).toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("public") || k == QStringLiteral("ispublic")) {
        if (rawData.contains(QStringLiteral("isPrivate"))) {
          return rawData.value(QStringLiteral("isPrivate")).toBool() ? QStringLiteral("false") : QStringLiteral("true");
        }
        return github.value(QStringLiteral("private")).toBool() ? QStringLiteral("false") : QStringLiteral("true");
      } else if (k == QStringLiteral("language")) {
        return github.value(QStringLiteral("language")).toString();
      }

      return QString();
    }

  private:
    QModelIndex m_index;
    const SourceModel *m_model;
  };

  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

    // First apply AST filtering if present
    if (m_ast) {
      ProxyFilterDataAccessor accessor(idx, qobject_cast<SourceModel *>(sourceModel()));
      if (!m_ast->evaluate(accessor)) {
        return false;
      }
    } else {
      if (!AdvancedFilterProxyModel::filterAcceptsRow(source_row, source_parent))
        return false;
    }

    return true;
  }

private:
  QSharedPointer<ASTNode> m_ast;
};

class BranchListProxyModel : public QAbstractListModel {
  Q_OBJECT
public:
  BranchListProxyModel(QSortFilterProxyModel *filterModel, NewSessionDialog *dialog, const QMultiMap<QString, QString> *selectedSources, bool showSelected, QObject *parent = nullptr)
      : QAbstractListModel(parent), m_filterModel(filterModel), m_dialog(dialog), m_selectedSources(selectedSources), m_showSelected(showSelected) {
    connect(m_filterModel, &QAbstractItemModel::modelReset, this, &BranchListProxyModel::rebuild);
    connect(m_filterModel, &QAbstractItemModel::rowsInserted, this, &BranchListProxyModel::rebuild);
    connect(m_filterModel, &QAbstractItemModel::rowsRemoved, this, &BranchListProxyModel::rebuild);
    connect(m_filterModel, &QAbstractItemModel::dataChanged, this, &BranchListProxyModel::rebuild);
    rebuild();
  }

  void updateSelection() {
    rebuild();
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid()) return 0;
    return m_rows.size();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    const auto &row = m_rows[index.row()];
    QModelIndex sourceIdx = row.sourceIdx;

    if (role == Qt::DisplayRole) {
      QString displayName = sourceIdx.siblingAtColumn(0).data(Qt::DisplayRole).toString();
      QString name = sourceIdx.data(SourceModel::NameRole).toString();

      int branchCount = 0;
      for (const auto &r : m_rows) {
        if (r.name == name) branchCount++;
      }

      if (branchCount > 1 || m_showSelected) {
        return displayName + QStringLiteral(" (") + row.branch + QStringLiteral(")");
      } else {
        return displayName;
      }
    }

    if (role == SourceModel::NameRole) {
      return row.name;
    }

    // Fallback to source model
    return sourceIdx.data(role);
  }

  QModelIndex mapToSource(const QModelIndex &proxyIndex) const {
    if (!proxyIndex.isValid() || proxyIndex.row() >= m_rows.size()) return QModelIndex();
    return m_rows[proxyIndex.row()].sourceIdx;
  }

  QString branch(const QModelIndex &proxyIndex) const {
    if (!proxyIndex.isValid() || proxyIndex.row() >= m_rows.size()) return QString();
    return m_rows[proxyIndex.row()].branch;
  }

private:
  void rebuild() {
    beginResetModel();
    m_rows.clear();
    for (int i = 0; i < m_filterModel->rowCount(); ++i) {
      QModelIndex filterIdx = m_filterModel->index(i, 0);
      QModelIndex baseIdx = m_filterModel->mapToSource(filterIdx);
      QString name = baseIdx.data(SourceModel::NameRole).toString();

      if (m_showSelected) {
        if (m_selectedSources->contains(name)) {
          QStringList branches = m_selectedSources->values(name);
          branches.sort();
          for (const QString &b : branches) {
            m_rows.push_back({baseIdx, name, b});
          }
        }
      } else {
        QStringList defaults = m_dialog->getDefaultBranches(baseIdx);
        for (const QString &b : defaults) {
          if (!m_selectedSources->contains(name) || !m_selectedSources->values(name).contains(b)) {
            m_rows.push_back({baseIdx, name, b});
          }
        }
      }
    }
    endResetModel();
  }

  struct RowData {
    QModelIndex sourceIdx;
    QString name;
    QString branch;
  };

  QSortFilterProxyModel *m_filterModel;
  NewSessionDialog *m_dialog;
  const QMultiMap<QString, QString> *m_selectedSources;
  bool m_showSelected;
  QVector<RowData> m_rows;
};"""

cpp = cpp.replace(old_proxy_class, new_proxy_class)


# Proxy init replacement
old_unselected_init = """  m_unselectedView = new QListView(this);
  m_unselectedProxy = new SourceSelectionProxyModel(&m_selectedSources, false, this);
  m_unselectedProxy->setSourceModel(m_sourceModel);
  m_unselectedProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_unselectedProxy->setFilterRole(SourceModel::NameRole);
  m_unselectedProxy->sort(0, Qt::DescendingOrder);"""

new_unselected_init = """  m_unselectedView = new QListView(this);
  m_unselectedFilterModel = new SourceFilterProxyModel(this);
  m_unselectedFilterModel->setSourceModel(m_sourceModel);
  m_unselectedFilterModel->setGlobalSourceModel(m_sourceModel);
  m_unselectedFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_unselectedFilterModel->setFilterRole(SourceModel::NameRole);
  m_unselectedFilterModel->sort(0, Qt::DescendingOrder);
  m_unselectedProxy = new BranchListProxyModel(m_unselectedFilterModel, this, &m_selectedSources, false, this);"""

cpp = cpp.replace(old_unselected_init, new_unselected_init)

old_selected_init = """  m_selectedView = new QListView(this);
  m_selectedProxy = new SourceSelectionProxyModel(&m_selectedSources, true, this);
  m_selectedProxy->setSourceModel(m_sourceModel);
  m_selectedProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_selectedProxy->setFilterRole(SourceModel::NameRole);
  m_selectedProxy->sort(0, Qt::DescendingOrder);"""

new_selected_init = """  m_selectedView = new QListView(this);
  m_selectedFilterModel = new SourceFilterProxyModel(this);
  m_selectedFilterModel->setSourceModel(m_sourceModel);
  m_selectedFilterModel->setGlobalSourceModel(m_sourceModel);
  m_selectedFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_selectedFilterModel->setFilterRole(SourceModel::NameRole);
  m_selectedFilterModel->sort(0, Qt::DescendingOrder);
  m_selectedProxy = new BranchListProxyModel(m_selectedFilterModel, this, &m_selectedSources, true, this);"""

cpp = cpp.replace(old_selected_init, new_selected_init)

# Other changes: context menu
old_context_menu = """    QAction *selectAction = menu.addAction(tr("Select"));
    connect(selectAction, &QAction::triggered, this, [this, proxyIdx]() {
      QModelIndexList selected = m_unselectedView->selectionModel()->selectedIndexes();
      if (!selected.contains(proxyIdx))
        selected = {proxyIdx};
      for (const QModelIndex &idx : selected) {
        QString name = idx.data(SourceModel::NameRole).toString();
        QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
        m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
      }
      updateModels();
    });

    QAction *filterAction = menu.addAction(tr("Filter just this"));
    connect(filterAction, &QAction::triggered, this, [this, name]() {
      m_filterEditor->setFilterText(name);
      applyFilter();
    });

    QString id = m_sourceModel->data(sourceIdx, SourceModel::IdRole).toString();"""

new_context_menu = """    QAction *selectAction = menu.addAction(tr("Select"));
    connect(selectAction, &QAction::triggered, this, [this, proxyIdx]() {
      QModelIndexList selected = m_unselectedView->selectionModel()->selectedIndexes();
      if (!selected.contains(proxyIdx))
        selected = {proxyIdx};
      for (const QModelIndex &idx : selected) {
        QString name = idx.data(SourceModel::NameRole).toString();
        QString b = m_unselectedProxy->branch(idx);
        if (!m_selectedSources.values(name).contains(b)) {
          m_selectedSources.insert(name, b);
        }
      }
      updateModels();
    });

    QAction *filterAction = menu.addAction(tr("Filter just this"));
    connect(filterAction, &QAction::triggered, this, [this, name]() {
      m_filterEditor->setFilterText(name);
      applyFilter();
    });

    QString id = m_sourceModel->data(sourceIdx, SourceModel::IdRole).toString();

    QMenu *defaultsMenu = menu.addMenu(tr("Default Branches"));
    QStringList availableBranches = getAvailableBranches(sourceIdx);
    QStringList currentDefaults = getDefaultBranches(sourceIdx);
    for (const QString &b : availableBranches) {
      QAction *bAction = defaultsMenu->addAction(b);
      bAction->setCheckable(true);
      bAction->setChecked(currentDefaults.contains(b));
      connect(bAction, &QAction::toggled, this, [this, b, id, sourceIdx](bool checked) {
        QStringList newDefaults = getDefaultBranches(sourceIdx);
        if (checked && !newDefaults.contains(b)) {
          newDefaults.append(b);
        } else if (!checked) {
          newDefaults.removeAll(b);
        }
        if (newDefaults.isEmpty()) {
          newDefaults.append(QStringLiteral("main"));
        }
        m_sourceModel->setDefaultBranches(id, newDefaults);
        updateModels();
      });
    }"""

cpp = cpp.replace(old_context_menu, new_context_menu)


old_applyFilter = """void NewSessionDialog::applyFilter() {
  QString text = m_filterEditor->filterText();
  QSharedPointer<ASTNode> ast;
  QString filterString;

  if (text.startsWith(QStringLiteral("="))) {
    ast = FilterParser::parse(text.mid(1));
  } else {
    filterString = text;
  }

  m_unselectedProxy->setFilterAST(ast);
  m_unselectedProxy->setFilterQuery(filterString);

  bool applyToSelected = m_selectedSources.size() >= 10;
  m_selectedProxy->setFilterAST(applyToSelected ? ast : QSharedPointer<ASTNode>());
  m_selectedProxy->setFilterQuery(applyToSelected ? filterString : QStringLiteral(""));
}"""

new_applyFilter = """void NewSessionDialog::applyFilter() {
  QString text = m_filterEditor->filterText();
  QSharedPointer<ASTNode> ast;
  QString filterString;

  if (text.startsWith(QStringLiteral("="))) {
    ast = FilterParser::parse(text.mid(1));
  } else {
    filterString = text;
  }

  m_unselectedFilterModel->setFilterAST(ast);
  m_unselectedFilterModel->setFilterQuery(filterString);

  bool applyToSelected = m_selectedSources.size() >= 10;
  m_selectedFilterModel->setFilterAST(applyToSelected ? ast : QSharedPointer<ASTNode>());
  m_selectedFilterModel->setFilterQuery(applyToSelected ? filterString : QStringLiteral(""));
}"""

cpp = cpp.replace(old_applyFilter, new_applyFilter)


old_onAddSelected = """void NewSessionDialog::onAddSelected() {
  QModelIndexList selection = m_unselectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
    m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
  }
  updateModels();
  m_unselectedView->clearSelection();
}"""

new_onAddSelected = """void NewSessionDialog::onAddSelected() {
  QModelIndexList selection = m_unselectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_unselectedProxy->branch(idx);
    if (!m_selectedSources.values(name).contains(b)) {
      m_selectedSources.insert(name, b);
    }
  }
  updateModels();
  m_unselectedView->clearSelection();
}"""

cpp = cpp.replace(old_onAddSelected, new_onAddSelected)


old_onRemoveSelected = """void NewSessionDialog::onRemoveSelected() {
  QModelIndexList selection = m_selectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    m_selectedSources.remove(idx.data(SourceModel::NameRole).toString());
  }
  updateModels();
  m_selectedView->clearSelection();
}"""

new_onRemoveSelected = """void NewSessionDialog::onRemoveSelected() {
  QModelIndexList selection = m_selectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_selectedProxy->branch(idx);
    m_selectedSources.remove(name, b);
  }
  updateModels();
  m_selectedView->clearSelection();
}"""

cpp = cpp.replace(old_onRemoveSelected, new_onRemoveSelected)


old_onSelectAll = """void NewSessionDialog::onSelectAll() {
  for (int i = 0; i < m_unselectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_unselectedProxy->index(i, 0);
    QString name = idx.data(SourceModel::NameRole).toString();
    QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
    m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
  }
  updateModels();
}"""

new_onSelectAll = """void NewSessionDialog::onSelectAll() {
  for (int i = 0; i < m_unselectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_unselectedProxy->index(i, 0);
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_unselectedProxy->branch(idx);
    if (!m_selectedSources.values(name).contains(b)) {
      m_selectedSources.insert(name, b);
    }
  }
  updateModels();
}"""

cpp = cpp.replace(old_onSelectAll, new_onSelectAll)


old_onUnselectAll = """void NewSessionDialog::onUnselectAll() {
  for (int i = 0; i < m_selectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_selectedProxy->index(i, 0);
    m_selectedSources.remove(idx.data(SourceModel::NameRole).toString());
  }
  updateModels();
}"""

new_onUnselectAll = """void NewSessionDialog::onUnselectAll() {
  for (int i = 0; i < m_selectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_selectedProxy->index(i, 0);
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_selectedProxy->branch(idx);
    m_selectedSources.remove(name, b);
  }
  updateModels();
}"""

cpp = cpp.replace(old_onUnselectAll, new_onUnselectAll)


old_returnPressed = """  connect(m_filterEditor, &FilterEditor::returnPressed, this, [this]() {
    if (m_unselectedProxy->rowCount() == 1) {
      QModelIndex idx = m_unselectedProxy->index(0, 0);
      QString name = idx.data(SourceModel::NameRole).toString();
      QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
      m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
      updateModels();
      m_filterEditor->clearLastTextToken();
    } else if (m_unselectedProxy->rowCount() > 1) {
      m_unselectedView->setFocus();
      m_unselectedView->setCurrentIndex(m_unselectedProxy->index(0, 0));
    }
  });"""

new_returnPressed = """  connect(m_filterEditor, &FilterEditor::returnPressed, this, [this]() {
    if (m_unselectedProxy->rowCount() == 1) {
      QModelIndex idx = m_unselectedProxy->index(0, 0);
      QString name = idx.data(SourceModel::NameRole).toString();
      QString b = m_unselectedProxy->branch(idx);
      if (!m_selectedSources.values(name).contains(b)) {
        m_selectedSources.insert(name, b);
      }
      updateModels();
      m_filterEditor->clearLastTextToken();
    } else if (m_unselectedProxy->rowCount() > 1) {
      m_unselectedView->setFocus();
      m_unselectedView->setCurrentIndex(m_unselectedProxy->index(0, 0));
    }
  });"""

cpp = cpp.replace(old_returnPressed, new_returnPressed)


old_unselectedView = """  connect(m_unselectedView, &QListView::activated, this, [this](const QModelIndex &idx) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
    m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
    updateModels();
    m_unselectedView->clearSelection();
    m_filterEditor->clearLastTextToken();
  });"""

new_unselectedView = """  connect(m_unselectedView, &QListView::activated, this, [this](const QModelIndex &idx) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_unselectedProxy->branch(idx);
    if (!m_selectedSources.values(name).contains(b)) {
      m_selectedSources.insert(name, b);
    }
    updateModels();
    m_unselectedView->clearSelection();
    m_filterEditor->clearLastTextToken();
  });"""

cpp = cpp.replace(old_unselectedView, new_unselectedView)


old_selectedView = """  connect(m_selectedView, &QListView::activated, this, [this](const QModelIndex &idx) {
    m_selectedSources.remove(idx.data(SourceModel::NameRole).toString());
    updateModels();
    m_selectedView->clearSelection();
  });"""

new_selectedView = """  connect(m_selectedView, &QListView::activated, this, [this](const QModelIndex &idx) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QString b = m_selectedProxy->branch(idx);
    m_selectedSources.remove(name, b);
    updateModels();
    m_selectedView->clearSelection();
  });"""

cpp = cpp.replace(old_selectedView, new_selectedView)


old_getDefaultBranch = """QString NewSessionDialog::getDefaultBranch(const QModelIndex &sourceIdx) {
  QJsonObject rawData = m_sourceModel->data(sourceIdx, SourceModel::RawDataRole).toJsonObject();

  QJsonObject githubRepo = rawData.value(QStringLiteral("githubRepo")).toObject();
  if (githubRepo.contains(QStringLiteral("defaultBranch"))) {
    QJsonObject db = githubRepo.value(QStringLiteral("defaultBranch")).toObject();
    if (db.contains(QStringLiteral("displayName"))) {
      return db.value(QStringLiteral("displayName")).toString();
    }
  }

  if (rawData.contains(QStringLiteral("defaultBranch"))) {
    return rawData.value(QStringLiteral("defaultBranch")).toString();
  }
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  if (github.contains(QStringLiteral("default_branch"))) {
    return github.value(QStringLiteral("default_branch")).toString();
  }
  return QStringLiteral("main");
}"""

new_getDefaultBranches = """QStringList NewSessionDialog::getDefaultBranches(const QModelIndex &sourceIdx) {
  QJsonObject rawData = m_sourceModel->data(sourceIdx, SourceModel::RawDataRole).toJsonObject();

  if (rawData.contains(QStringLiteral("local_defaultBranches"))) {
    QStringList defaults;
    QJsonArray arr = rawData.value(QStringLiteral("local_defaultBranches")).toArray();
    for (const QJsonValue &v : arr) {
      defaults.append(v.toString());
    }
    if (!defaults.isEmpty()) {
      return defaults;
    }
  }

  QJsonObject githubRepo = rawData.value(QStringLiteral("githubRepo")).toObject();
  if (githubRepo.contains(QStringLiteral("defaultBranch"))) {
    QJsonObject db = githubRepo.value(QStringLiteral("defaultBranch")).toObject();
    if (db.contains(QStringLiteral("displayName"))) {
      return QStringList{db.value(QStringLiteral("displayName")).toString()};
    }
  }

  if (rawData.contains(QStringLiteral("defaultBranch"))) {
    return QStringList{rawData.value(QStringLiteral("defaultBranch")).toString()};
  }
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  if (github.contains(QStringLiteral("default_branch"))) {
    return QStringList{github.value(QStringLiteral("default_branch")).toString()};
  }
  return QStringList{QStringLiteral("main")};
}"""

cpp = cpp.replace(old_getDefaultBranch, new_getDefaultBranches)


old_getAvailableBranches = """  // Determine default branch
  QString defaultBranch = getDefaultBranch(sourceIdx);

  // If no branches known, fallback to defaultBranch and some standard ones
  if (branches.isEmpty()) {
    if (!defaultBranch.isEmpty()) {
      addUnique(QJsonArray{defaultBranch});
    }
    addUnique(QJsonArray{QStringLiteral("main"), QStringLiteral("master")});
  }

  // Ensure default branch is at the top
  QString topBranch;
  if (!defaultBranch.isEmpty() && seen.contains(defaultBranch)) {
    topBranch = defaultBranch;
  } else if (seen.contains(QStringLiteral("main"))) {
    topBranch = QStringLiteral("main");
  } else if (seen.contains(QStringLiteral("master"))) {
    topBranch = QStringLiteral("master");
  }

  if (!topBranch.isEmpty()) {
    branches.removeAll(topBranch);
    branches.prepend(topBranch);
  }

  return branches;
}"""

new_getAvailableBranches = """  // Determine default branches
  QStringList defaultBranches = getDefaultBranches(sourceIdx);

  // If no branches known, fallback to defaults and some standard ones
  if (branches.isEmpty()) {
    for (const QString &b : defaultBranches) {
      addUnique(QJsonArray{b});
    }
    addUnique(QJsonArray{QStringLiteral("main"), QStringLiteral("master")});
  }

  // Ensure default branches are at the top
  for (int i = defaultBranches.size() - 1; i >= 0; --i) {
    QString b = defaultBranches[i];
    if (seen.contains(b)) {
      branches.removeAll(b);
      branches.prepend(b);
    }
  }

  if (branches.isEmpty()) {
    if (seen.contains(QStringLiteral("main"))) {
      branches.removeAll(QStringLiteral("main"));
      branches.prepend(QStringLiteral("main"));
    } else if (seen.contains(QStringLiteral("master"))) {
      branches.removeAll(QStringLiteral("master"));
      branches.prepend(QStringLiteral("master"));
    }
  }

  return branches;
}"""

cpp = cpp.replace(old_getAvailableBranches, new_getAvailableBranches)


old_setInitialData = """  for (const QJsonValue &sVal : sourcesArr) {
    QJsonObject sObj = sVal.toObject();
    QString name = sObj.value(QStringLiteral("name")).toString();
    QString branch = sObj.value(QStringLiteral("branch")).toString();
    if (!name.isEmpty()) {
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        if (branch.isEmpty()) {
          m_selectedSources.insert(name, getDefaultBranch(matches.first()));
        } else {
          m_selectedSources.insert(name, branch);
        }
      } else {
        // Source not found, add to custom
        QJsonArray arr;
        QJsonObject obj;
        obj[QStringLiteral("id")] = name;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("isCustom")] = true;
        arr.append(obj);
        m_sourceModel->addSources(arr);
        m_selectedSources.insert(name, branch.isEmpty() ? QStringLiteral("main") : branch);
      }
    }
  }"""

new_setInitialData = """  for (const QJsonValue &sVal : sourcesArr) {
    QJsonObject sObj = sVal.toObject();
    QString name = sObj.value(QStringLiteral("name")).toString();
    QString branch = sObj.value(QStringLiteral("branch")).toString();
    if (!name.isEmpty()) {
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        if (branch.isEmpty()) {
          QStringList defaults = getDefaultBranches(matches.first());
          m_selectedSources.insert(name, defaults.isEmpty() ? QStringLiteral("main") : defaults.first());
        } else {
          m_selectedSources.insert(name, branch);
        }
      } else {
        // Source not found, add to custom
        QJsonArray arr;
        QJsonObject obj;
        obj[QStringLiteral("id")] = name;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("isCustom")] = true;
        arr.append(obj);
        m_sourceModel->addSources(arr);
        m_selectedSources.insert(name, branch.isEmpty() ? QStringLiteral("main") : branch);
      }
    }
  }"""

cpp = cpp.replace(old_setInitialData, new_setInitialData)


old_setInitialData_2 = """  } else if (data.contains(QStringLiteral("source"))) {
    QString name = data.value(QStringLiteral("source")).toString();
    QString branch = data.value(QStringLiteral("startingBranch")).toString();
    if (branch.isEmpty()) {
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        branch = getDefaultBranch(matches.first());
      } else {
        branch = QStringLiteral("main");
      }
    }
    m_selectedSources.insert(name, branch);"""

new_setInitialData_2 = """  } else if (data.contains(QStringLiteral("source"))) {
    QString name = data.value(QStringLiteral("source")).toString();
    QString branch = data.value(QStringLiteral("startingBranch")).toString();
    if (branch.isEmpty()) {
      QModelIndexList matches =
          m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        QStringList defaults = getDefaultBranches(matches.first());
        branch = defaults.isEmpty() ? QStringLiteral("main") : defaults.first();
      } else {
        branch = QStringLiteral("main");
      }
    }
    m_selectedSources.insert(name, branch);"""

cpp = cpp.replace(old_setInitialData_2, new_setInitialData_2)


old_eventFilter = """  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    auto focusList = [](QListView *view, QSortFilterProxyModel *proxy) {
      view->setFocus();
      if (!view->currentIndex().isValid() && proxy->rowCount() > 0) {
        view->setCurrentIndex(proxy->index(0, 0));
      }
      return true;
    };"""

new_eventFilter = """  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    auto focusList = [](QListView *view, QAbstractItemModel *proxy) {
      view->setFocus();
      if (!view->currentIndex().isValid() && proxy->rowCount() > 0) {
        view->setCurrentIndex(proxy->index(0, 0));
      }
      return true;
    };"""

cpp = cpp.replace(old_eventFilter, new_eventFilter)

cpp += """\n#include "newsessiondialog.moc"\n"""

with open('src/newsessiondialog.cpp', 'w') as f:
    f.write(cpp)
