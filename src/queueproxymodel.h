#ifndef QUEUEPROXYMODEL_H
#define QUEUEPROXYMODEL_H

#include <QHash>
#include <QSortFilterProxyModel>

class QueueProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit QueueProxyModel(QObject *parent = nullptr);

  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

#endif // QUEUEPROXYMODEL_H
