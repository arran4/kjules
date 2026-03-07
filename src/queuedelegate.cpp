#include "queuedelegate.h"
#include "queuemodel.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QTextDocument>

QueueDelegate::QueueDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void QueueDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  painter->save();

  if (opt.state & QStyle::State_Selected) {
    painter->fillRect(opt.rect, opt.palette.highlight());
    painter->setPen(opt.palette.highlightedText().color());
  } else {
    painter->fillRect(opt.rect, opt.palette.base());
    painter->setPen(opt.palette.text().color());
  }

  QString summary = index.data(QueueModel::SummaryRole).toString();
  QString status = index.data(QueueModel::StatusRole).toString();
  int errorCount = index.data(QueueModel::ErrorCountRole).toInt();

  QRect rect = opt.rect.adjusted(5, 5, -5, -5);

  QFont font = opt.font;
  font.setBold(true);
  painter->setFont(font);

  QRect summaryRect;
  painter->drawText(rect, Qt::AlignLeft | Qt::AlignTop, summary, &summaryRect);

  font.setBold(false);
  painter->setFont(font);

  if (errorCount > 0) {
    painter->setPen(Qt::red);
  } else {
    painter->setPen(opt.palette.text().color());
  }

  QRect statusRect = rect;
  statusRect.setTop(summaryRect.bottom() + 2);
  painter->drawText(statusRect, Qt::AlignLeft | Qt::AlignTop, status);

  painter->restore();
}

QSize QueueDelegate::sizeHint(const QStyleOptionViewItem &option,
                              const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  return QSize(200, 50); // fixed size for simplicity
}
