#ifndef TEMPLATEDELEGATE_H
#define TEMPLATEDELEGATE_H

#include <QStyledItemDelegate>

class TemplateDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit TemplateDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

#endif // TEMPLATEDELEGATE_H
