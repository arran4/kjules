#ifndef SOURCEDELEGATE_H
#define SOURCEDELEGATE_H

#include <QStyledItemDelegate>

class SourceDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit SourceDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

Q_SIGNALS:
  void actionClicked(const QModelIndex &index, int actionId);
};

#endif // SOURCEDELEGATE_H
