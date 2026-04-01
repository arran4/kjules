#include "draftdelegate.h"
#include "draftsmodel.h"
#include "templatesmodel.h"
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

  QString displayTitle = index.data(Qt::DisplayRole).toString();
  if (displayTitle.isEmpty()) {
    displayTitle = index.data(DraftsModel::PromptRole).toString();
  }

  QString prompt = index.data(DraftsModel::PromptRole).toString();
  QString automationMode =
      index.data(DraftsModel::AutomationModeRole).toString();
  QString source = index.data(DraftsModel::SourceRole).toString();

  // Try to get comment/description to show in details
  QString comment = index.data(DraftsModel::CommentRole).toString();
  QString description = index.data(TemplatesModel::DescriptionRole).toString();

  QRect r = option.rect.adjusted(5, 5, -5, -5);

  // Title
  QFont promptFont = option.font;
  promptFont.setBold(true);
  painter->setFont(promptFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.text().color());

  QString displayTitleShort = displayTitle.left(50);
  if (displayTitle.length() > 50)
    displayTitleShort += QStringLiteral("...");

  QRect promptRect =
      painter->boundingRect(r, Qt::AlignLeft | Qt::AlignTop, displayTitleShort);
  painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, displayTitleShort);

  // Details
  QFont detailsFont = option.font;
  detailsFont.setPointSize(detailsFont.pointSize() - 1);
  painter->setFont(detailsFont);
  painter->setPen(option.state & QStyle::State_Selected
                      ? option.palette.highlightedText().color()
                      : option.palette.placeholderText().color());

  QString details = QStringLiteral("Draft / Template");
  if (!comment.isEmpty() && comment != displayTitle)
    details += QStringLiteral(" | ") + comment;
  if (!description.isEmpty())
    details += QStringLiteral(" | ") + description;
  if (comment.isEmpty() && description.isEmpty() && prompt != displayTitle)
    details +=
        QStringLiteral(" | Prompt: ") + prompt.left(30) +
        (prompt.length() > 30 ? QStringLiteral("...") : QStringLiteral(""));

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
