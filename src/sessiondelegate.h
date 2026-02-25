#ifndef SESSIONDELEGATE_H
#define SESSIONDELEGATE_H

#include <QStyledItemDelegate>

class SessionDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit SessionDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

#endif  // SESSIONDELEGATE_H
