#ifndef DRAFTDELEGATE_H
#define DRAFTDELEGATE_H

#include <QStyledItemDelegate>

class DraftDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit DraftDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

#endif  // DRAFTDELEGATE_H
