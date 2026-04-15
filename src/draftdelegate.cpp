#include "draftdelegate.h"
#include "draftsmodel.h"
#include "errorsmodel.h"
#include "templatesmodel.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
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

  QVariant requestVar = index.data(ErrorsModel::RequestRole);
  if (requestVar.isValid() && !requestVar.isNull()) {
    QJsonObject reqObj = requestVar.toJsonObject();
    if (prompt.isEmpty()) {
      prompt = reqObj.value(QStringLiteral("prompt")).toString();
    }
    if (source.isEmpty()) {
      if (reqObj.contains(QStringLiteral("sources"))) {
        QJsonArray sourcesArray =
            reqObj.value(QStringLiteral("sources")).toArray();
        QStringList sourcesList;
        for (const QJsonValue &val : sourcesArray) {
          sourcesList.append(val.toString());
        }
        source = sourcesList.join(QStringLiteral(", "));
      } else {
        source = reqObj.value(QStringLiteral("source")).toString();
      }
    }
  }

  // Try to get comment/description to show in details
  QString comment = index.data(DraftsModel::CommentRole).toString();
  QString description = index.data(TemplatesModel::DescriptionRole).toString();
  QString timestamp = index.data(ErrorsModel::TimestampRole).toString();

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
  if (comment.isEmpty() && description.isEmpty() && prompt != displayTitle &&
      !prompt.isEmpty()) {
    QString shortPrompt = prompt;
    shortPrompt.replace(QStringLiteral("\n"), QStringLiteral(" "));
    details += QStringLiteral(" | Prompt: ") + shortPrompt.left(30) +
               (shortPrompt.length() > 30 ? QStringLiteral("...")
                                          : QStringLiteral(""));
  }

  if (!automationMode.isEmpty())
    details += QStringLiteral(" | Auto: ") + automationMode;
  if (!source.isEmpty())
    details += QStringLiteral(" | Source: ") + source;
  if (!timestamp.isEmpty())
    details += QStringLiteral(" | ") + timestamp;

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
