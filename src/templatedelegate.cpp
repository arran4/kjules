#include "templatedelegate.h"
#include "templatesmodel.h"

#include <QPainter>
#include <QStyleOptionViewItem>

TemplateDelegate::TemplateDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void TemplateDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  painter->save();

  if (opt.state & QStyle::State_Selected) {
    painter->fillRect(opt.rect, opt.palette.highlight());
    painter->setPen(opt.palette.highlightedText().color());
  } else {
    painter->setPen(opt.palette.text().color());
  }

  QString prompt = index.data(TemplatesModel::PromptRole).toString();
  QString automationMode =
      index.data(TemplatesModel::AutomationModeRole).toString();

  QRect r = opt.rect.adjusted(5, 5, -5, -5);

  QFont font = painter->font();
  font.setBold(true);
  painter->setFont(font);

  QString display = prompt;
  display.replace(QLatin1Char('\n'), QLatin1Char(' '));
  if (display.length() > 50) {
    display = display.left(47) + QStringLiteral("...");
  }
  if (display.isEmpty()) {
    display = QStringLiteral("Empty Template");
  }

  painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, display);

  font.setBold(false);
  font.setPointSize(font.pointSize() - 1);
  painter->setFont(font);

  int yOffset = painter->fontMetrics().height() + 5;
  r.adjust(0, yOffset, 0, 0);

  QString modeText = automationMode.isEmpty() ? QStringLiteral("Standard Mode")
                                              : automationMode;
  painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, modeText);

  painter->restore();
}

QSize TemplateDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  Q_UNUSED(index)
  QFontMetrics fm(option.font);
  return QSize(200, fm.height() * 2 + 15);
}
