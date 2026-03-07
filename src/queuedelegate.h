#ifndef QUEUEDELEGATE_H
#define QUEUEDELEGATE_H

#include <QStyledItemDelegate>

class QueueDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit QueueDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

#endif // QUEUEDELEGATE_H
