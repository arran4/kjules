#include "sourcedelegate.h"
#include "sourcemodel.h"
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

SourceDelegate::SourceDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void SourceDelegate::paint(QPainter *painter,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) const {
  painter->save();

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
    painter->setPen(option.palette.highlightedText().color());
  } else {
    painter->fillRect(option.rect, option.palette.base());
    painter->setPen(option.palette.text().color());
  }

  // Define action button areas
  int actionWidth = 80;
  int moreWidth = 30;
  int margin = 5;

  QRect rect = option.rect.adjusted(
      margin, margin, -actionWidth - moreWidth - margin * 3, -margin);

  QString name = index.data(SourceModel::NameRole).toString();
  QString type = index.data(SourceModel::TypeRole).toString();
  QString description = index.data(SourceModel::DescriptionRole).toString();

  bool isNew = index.data(SourceModel::IsNewRole).toBool();
  QFont font = option.font;

  if (isNew) {
    if (!(option.state & QStyle::State_Selected)) {
      painter->fillRect(option.rect, QColor(255, 255, 200));
      painter->setPen(option.palette.text().color());
    }
    font.setBold(true);
  }

  painter->setFont(font);
  QRect nameRect = rect;
  painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignTop, name);

  QFontMetrics fm(font);
  int nameHeight = fm.height();

  if (!description.isEmpty()) {
    QFont descFont = font;
    descFont.setPointSize(font.pointSize() - 2);
    descFont.setBold(false);
    painter->setFont(descFont);

    QRect descRect = rect.adjusted(0, nameHeight + 2, 0, 0);
    painter->drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      description);
  }

  // Draw action buttons
  QRect newSessionRect(
      option.rect.right() - actionWidth - moreWidth - margin * 2,
      option.rect.top() + (option.rect.height() - 24) / 2, actionWidth, 24);

  QStyleOptionButton newBtnOpt;
  newBtnOpt.rect = newSessionRect;
  newBtnOpt.text = QStringLiteral("New Session");
  newBtnOpt.state = QStyle::State_Enabled;
  QApplication::style()->drawControl(QStyle::CE_PushButton, &newBtnOpt,
                                     painter);

  QRect moreRect(option.rect.right() - moreWidth - margin,
                 option.rect.top() + (option.rect.height() - 24) / 2, moreWidth,
                 24);

  QStyleOptionButton moreBtnOpt;
  moreBtnOpt.rect = moreRect;
  moreBtnOpt.text = QStringLiteral("...");
  moreBtnOpt.state = QStyle::State_Enabled;
  QApplication::style()->drawControl(QStyle::CE_PushButton, &moreBtnOpt,
                                     painter);

  painter->restore();
}

QSize SourceDelegate::sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex & /*index*/) const {
  return QSize(option.rect.width(), 50);
}

bool SourceDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) {
  if (event->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

    int actionWidth = 80;
    int moreWidth = 30;
    int margin = 5;

    QRect newSessionRect(
        option.rect.right() - actionWidth - moreWidth - margin * 2,
        option.rect.top() + (option.rect.height() - 24) / 2, actionWidth, 24);

    QRect moreRect(option.rect.right() - moreWidth - margin,
                   option.rect.top() + (option.rect.height() - 24) / 2,
                   moreWidth, 24);

    if (newSessionRect.contains(mouseEvent->pos())) {
      Q_EMIT actionClicked(index, 0); // 0 for New Session
      return true;
    } else if (moreRect.contains(mouseEvent->pos())) {
      Q_EMIT actionClicked(index, 1); // 1 for More options
      return true;
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}
