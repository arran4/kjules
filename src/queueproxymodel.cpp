#include "queueproxymodel.h"
#include "queuemodel.h"

QueueProxyModel::QueueProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

bool QueueProxyModel::filterAcceptsRow(int source_row,
                                       const QModelIndex &source_parent) const {
  const auto *model = qobject_cast<const QueueModel *>(sourceModel());
  if (!model) {
    return true;
  }
  return !model->getItem(source_row).isBlocked;
}
