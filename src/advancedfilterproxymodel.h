#ifndef ADVANCEDFILTERPROXYMODEL_H
#define ADVANCEDFILTERPROXYMODEL_H

#include "filterparser.h"
#include <QSharedPointer>
#include <QSortFilterProxyModel>

class SourceModel;

class AdvancedFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit AdvancedFilterProxyModel(QObject *parent = nullptr);

  void setFilterQuery(const QString &query);
  void setGlobalSourceModel(SourceModel *sourceModel);
  QSharedPointer<ASTNode> filterAST() const { return m_ast; }
  QString filterQuery() const { return m_query; }

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
  bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
  QString m_query;
  QSharedPointer<ASTNode> m_ast;
  SourceModel *m_globalSourceModel = nullptr;
};

class FollowingFilterProxyModel : public AdvancedFilterProxyModel {
  Q_OBJECT
public:
  enum TabType { FollowingTab, SnoozedTab };
  explicit FollowingFilterProxyModel(QObject *parent = nullptr);

  void setTabType(TabType type);

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  TabType m_tabType = FollowingTab;
};

#endif // ADVANCEDFILTERPROXYMODEL_H
