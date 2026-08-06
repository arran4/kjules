import re

with open('src/newsessiondialog.cpp', 'r') as f:
    content = f.read()

# Replace the entire SourceSelectionProxyModel class

match = re.search(r"class SourceSelectionProxyModel.*?private:\n  const QMultiMap<QString, QString> \*m_selectedSources;\n  bool m_showSelected;\n  QSharedPointer<ASTNode> m_ast;\n};\n", content, re.DOTALL)
if match:
    old_class = match.group(0)
else:
    print("Could not find SourceSelectionProxyModel")
    exit(1)

new_class = """class SourceFilterProxyModel : public AdvancedFilterProxyModel {
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
};
"""

content = content.replace(old_class, new_class)

with open('src/newsessiondialog.cpp', 'w') as f:
    f.write(content)
