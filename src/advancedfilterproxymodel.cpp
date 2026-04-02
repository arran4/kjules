#include "advancedfilterproxymodel.h"
#include <QStringList>

class ModelDataAccessor : public FilterDataAccessor {
public:
  ModelDataAccessor(QAbstractItemModel *m, int r, const QModelIndex &p)
      : model(m), row(r), parent(p) {}

  QString getValue(const QString &key) const override {
    // Try to match column header with the key.
    for (int c = 0; c < model->columnCount(parent); ++c) {
      QString header = model->headerData(c, Qt::Horizontal, Qt::DisplayRole)
                           .toString()
                           .remove(QLatin1Char(' '))
                           .toLower();
      if (header == key.toLower() ||
          model->headerData(c, Qt::Horizontal, Qt::DisplayRole)
                  .toString()
                  .toLower() == key.toLower()) {
        QModelIndex idx = model->index(row, c, parent);
        return model->data(idx, Qt::DisplayRole).toString();
      }
    }
    // Fallback or custom keys mapping if needed
    return QString();
  }

  QList<QString> getAllValues() const override {
    QList<QString> vals;
    for (int c = 0; c < model->columnCount(parent); ++c) {
      QModelIndex idx = model->index(row, c, parent);
      vals.append(model->data(idx, Qt::DisplayRole).toString());
    }
    return vals;
  }

private:
  QAbstractItemModel *model;
  int row;
  QModelIndex parent;
};

AdvancedFilterProxyModel::AdvancedFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
  setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void AdvancedFilterProxyModel::setFilterQuery(const QString &query) {
  m_query = query.trimmed();
  if (m_query.startsWith(QLatin1String("="))) {
    m_ast = FilterParser::parse(m_query);
  } else {
    m_ast.reset();
    setFilterFixedString(m_query);
  }
  invalidateFilter();
}

bool AdvancedFilterProxyModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
  if (m_query.isEmpty())
    return true;

  if (m_query.startsWith(QLatin1String("=")) && m_ast) {
    ModelDataAccessor accessor(sourceModel(), source_row, source_parent);
    return m_ast->evaluate(accessor);
  }

  // Default global substring search across all columns.
  QAbstractItemModel *m = sourceModel();
  int cols = m->columnCount(source_parent);
  for (int c = 0; c < cols; ++c) {
    QModelIndex idx = m->index(source_row, c, source_parent);
    QString val = m->data(idx, Qt::DisplayRole).toString();
    if (val.contains(m_query, filterCaseSensitivity())) {
      return true;
    }
  }
  return false;
}
