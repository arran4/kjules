#ifndef ADVANCEDFILTERPROXYMODEL_H
#define ADVANCEDFILTERPROXYMODEL_H

#include "filterparser.h"
#include <QSharedPointer>
#include <QSortFilterProxyModel>

class AdvancedFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit AdvancedFilterProxyModel(QObject *parent = nullptr);

  void setFilterQuery(const QString &query);

protected:
  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override;
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override;

private:
  QString m_query;
  QSharedPointer<ASTNode> m_ast;
};

#endif // ADVANCEDFILTERPROXYMODEL_H
