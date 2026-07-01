#include "queueproxymodel.h"
#include "queuemodel.h"

QueueProxyModel::QueueProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

bool QueueProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  bool isBlocked = sourceModel()->data(idx, QueueModel::Roles::StatusRole).toString() == QStringLiteral("Blocked");
  return !isBlocked;
}
