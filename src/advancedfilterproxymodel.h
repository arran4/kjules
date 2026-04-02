#ifndef ADVANCEDFILTERPROXYMODEL_H
#define ADVANCEDFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QSharedPointer>
#include "filterparser.h"

class AdvancedFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit AdvancedFilterProxyModel(QObject *parent = nullptr);

    void setFilterQuery(const QString &query);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString m_query;
    QSharedPointer<ASTNode> m_ast;
};

#endif // ADVANCEDFILTERPROXYMODEL_H
