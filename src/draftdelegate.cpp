#include "draftdelegate.h"
#include "draftsmodel.h"
#include <QDebug>
#include <QJsonArray>
#include <QPainter>

DraftDelegate::DraftDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void DraftDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const {
  if (!index.isValid())
    return;

  painter->save();

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  }

  QString prompt = index.data(DraftsModel::PromptRole).toString();
  QString automationMode =
      index.data(DraftsModel::AutomationModeRole).toString();
  QString source = index.data(DraftsModel::SourceRole).toString();

  // Let's assume prompt is primary identifier.

  QRect r = option.rect.adjusted(5, 5, -5, -5);

  // Prompt (Title)
  QFont promptFont = option.font;
  promptFont.setBold(true);
  painter->setFont(promptFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.text().color());

  QString displayPrompt = prompt.left(50);
  if (prompt.length() > 50)
    displayPrompt += QStringLiteral("...");

  QRect promptRect =
      painter->boundingRect(r, Qt::AlignLeft | Qt::AlignTop, displayPrompt);
  painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, displayPrompt);

  // Details
  QFont detailsFont = option.font;
  detailsFont.setPointSize(detailsFont.pointSize() - 1);
  painter->setFont(detailsFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.placeholderText().color());

  QString details = QStringLiteral("Draft");
  if (!automationMode.isEmpty())
    details += QStringLiteral(" | Auto: ") + automationMode;
  if (!source.isEmpty())
    details += QStringLiteral(" | Source: ") + source;

  QRect detailsRect = r;
  detailsRect.setTop(promptRect.bottom() + 5);
  painter->drawText(detailsRect, Qt::AlignLeft | Qt::AlignTop, details);

  painter->restore();
}

QSize DraftDelegate::sizeHint(const QStyleOptionViewItem &option,
                              const QModelIndex &index) const {
  Q_UNUSED(index)
  return QSize(option.rect.width(), 50);
}
