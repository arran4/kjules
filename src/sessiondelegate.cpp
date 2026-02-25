#include "sessiondelegate.h"

#include <QDebug>
#include <QPainter>

#include "sessionmodel.h"

SessionDelegate::SessionDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void SessionDelegate::paint(QPainter *painter,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
  if (!index.isValid()) return;

  painter->save();

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  }

  QString title = index.data(SessionModel::TitleRole).toString();
  QString source = index.data(SessionModel::SourceRole).toString();
  QString id = index.data(SessionModel::IdRole).toString();

  // Fallback if title is empty
  if (title.isEmpty()) title = id;

  QRect r = option.rect.adjusted(5, 5, -5, -5);

  // Title
  QFont titleFont = option.font;
  titleFont.setBold(true);
  titleFont.setPointSize(titleFont.pointSize() + 2);
  painter->setFont(titleFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.text().color());

  QRect titleRect =
      painter->boundingRect(r, Qt::AlignLeft | Qt::AlignTop, title);
  painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, title);

  // Source
  QFont sourceFont = option.font;
  sourceFont.setPointSize(sourceFont.pointSize() - 1);
  painter->setFont(sourceFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.placeholderText().color());

  QRect sourceRect = r;
  sourceRect.setTop(titleRect.bottom() + 5);
  painter->drawText(sourceRect, Qt::AlignLeft | Qt::AlignTop,
                    "Source: " + source);

  painter->restore();
}

QSize SessionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  return QSize(option.rect.width(), 60);
}
