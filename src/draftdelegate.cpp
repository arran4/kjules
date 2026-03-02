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
  // Assuming source is stored as list in new logic, but DraftsModel::SourceRole
  // might return string or list. In DraftsModel::data, SourceRole returns
  // "source". I should update DraftsModel to handle "sources" array. For now,
  // let's assume it returns whatever was stored. If I updated NewSessionDialog
  // to store "sources" array, DraftsModel::data(SourceRole) might return
  // QVariant(QJsonArray) or QVariantList. Let's check DraftsModel::data
  // implementation.

  // In DraftsModel::data: return draft.value("source").toString();
  // If "sources" exists, "source" might be missing.
  // I need to update DraftsModel to return sources list or joined string.

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
