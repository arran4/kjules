#include "advancedfilterproxymodel.h"
#include "sessionmodel.h"
#include "sourcemodel.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

class ModelDataAccessor : public FilterDataAccessor {
public:
  ModelDataAccessor(QAbstractItemModel *m, int r, const QModelIndex &p, SourceModel *gsm)
      : model(m), row(r), parent(p), globalSourceModel(gsm) {}

  QString getValue(const QString &key) const override {
    QString lowerKey = key.toLower();
    static const QHash<QString, int> keyToColumn = {{QStringLiteral("language"), SourceModel::ColLanguages},
                                                    {QStringLiteral("fork"), SourceModel::ColFork},
                                                    {QStringLiteral("private"), SourceModel::ColPrivate},
                                                    {QStringLiteral("public"), SourceModel::ColPrivate},
                                                    {QStringLiteral("archived"), SourceModel::ColArchived}};

    auto getBoolStr = [](const QJsonObject &rawData, const QString &k) {
      QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
      if (k == QStringLiteral("private")) {
        bool isPrivate = rawData.contains(QStringLiteral("isPrivate"))
                             ? rawData.value(QStringLiteral("isPrivate")).toBool()
                             : github.value(QStringLiteral("private")).toBool();
        return isPrivate ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("public")) {
        bool isPrivate = rawData.contains(QStringLiteral("isPrivate"))
                             ? rawData.value(QStringLiteral("isPrivate")).toBool()
                             : github.value(QStringLiteral("private")).toBool();
        return !isPrivate ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("fork")) {
        bool isFork = rawData.contains(QStringLiteral("isFork")) ? rawData.value(QStringLiteral("isFork")).toBool()
                                                                 : github.value(QStringLiteral("fork")).toBool();
        return isFork ? QStringLiteral("true") : QStringLiteral("false");
      } else if (k == QStringLiteral("archived")) {
        bool isArchived = rawData.contains(QStringLiteral("isArchived"))
                              ? rawData.value(QStringLiteral("isArchived")).toBool()
                              : github.value(QStringLiteral("archived")).toBool();
        return isArchived ? QStringLiteral("true") : QStringLiteral("false");
      }
      return QString();
    };

    if (qobject_cast<SessionModel *>(model) &&
        (lowerKey == QStringLiteral("label") || lowerKey == QStringLiteral("labels"))) {
      return model->data(model->index(row, SessionModel::ColPRLabels, parent), Qt::DisplayRole).toString();
    }

    if (qobject_cast<SourceModel *>(model) && keyToColumn.contains(lowerKey)) {
      if (lowerKey == QStringLiteral("private") || lowerKey == QStringLiteral("public") ||
          lowerKey == QStringLiteral("fork") || lowerKey == QStringLiteral("archived")) {
        QJsonObject rawData = model->data(model->index(row, 0, parent), SourceModel::RawDataRole).toJsonObject();
        return getBoolStr(rawData, lowerKey);
      }
      return model->data(model->index(row, keyToColumn.value(lowerKey), parent), Qt::DisplayRole).toString();
    } else if (qobject_cast<SessionModel *>(model) && keyToColumn.contains(lowerKey) && globalSourceModel) {
      QString sourceId =
          model->data(model->index(row, SessionModel::ColTitle, parent), SessionModel::SourceRole).toString();
      QModelIndexList matches = globalSourceModel->match(globalSourceModel->index(0, SourceModel::ColName),
                                                         SourceModel::IdRole, sourceId, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        QModelIndex sourceIdx = matches.first();
        if (lowerKey == QStringLiteral("private") || lowerKey == QStringLiteral("public") ||
            lowerKey == QStringLiteral("fork") || lowerKey == QStringLiteral("archived")) {
          QJsonObject rawData =
              globalSourceModel->data(globalSourceModel->index(sourceIdx.row(), 0), SourceModel::RawDataRole)
                  .toJsonObject();
          return getBoolStr(rawData, lowerKey);
        }
        return globalSourceModel
            ->data(globalSourceModel->index(sourceIdx.row(), keyToColumn.value(lowerKey)), Qt::DisplayRole)
            .toString();
      }
    }

    // Try to match column header with the key.
    for (int c = 0; c < model->columnCount(parent); ++c) {
      QString header =
          model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().remove(QLatin1Char(' ')).toLower();
      if ((key.toLower() == QStringLiteral("repo") || key.toLower() == QStringLiteral("owner")) &&
          header == QStringLiteral("name")) {
        QModelIndex idx = model->index(row, c, parent);
        QString fullName = model->data(idx, Qt::DisplayRole).toString();
        // If the name is "owner/repo", we extract the relevant part.
        if (fullName.contains(QLatin1Char('/'))) {
          if (key.toLower() == QStringLiteral("owner")) {
            return fullName.section(QLatin1Char('/'), 0, 0);
          } else if (key.toLower() == QStringLiteral("repo")) {
            return fullName.section(QLatin1Char('/'), 1, 1);
          }
        }
        return fullName;
      }
      if (header == key.toLower() ||
          model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().toLower() == key.toLower()) {
        QModelIndex idx = model->index(row, c, parent);
        return model->data(idx, Qt::DisplayRole).toString();
      }
    }
    // Fallback or custom keys mapping if needed
    return QString();
  }

  QList<QString> getAllValues() const override {
    QList<QString> vals;
    for (int c = 0; c < model->columnCount(parent); ++c) {
      QModelIndex idx = model->index(row, c, parent);
      vals.append(model->data(idx, Qt::DisplayRole).toString());
    }
    return vals;
  }

private:
  QAbstractItemModel *model;
  int row;
  QModelIndex parent;
  SourceModel *globalSourceModel;
};

AdvancedFilterProxyModel::AdvancedFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
  setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void AdvancedFilterProxyModel::setGlobalSourceModel(SourceModel *sourceModel) { m_globalSourceModel = sourceModel; }

void AdvancedFilterProxyModel::setFilterQuery(const QString &query) {
  const QString normalizedQuery = query.trimmed();
  if (m_query == normalizedQuery)
    return;

  m_query = normalizedQuery;
  if (m_query.startsWith(QLatin1String("="))) {
    m_ast = FilterParser::parse(m_query.mid(1).trimmed());
  } else {
    m_ast.reset();
  }
  invalidate();
}

bool AdvancedFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  const QString trimmedQuery = m_query.trimmed();
  if (trimmedQuery.isEmpty())
    return true;

  if (trimmedQuery.startsWith(QLatin1String("="))) {
    if (m_ast) {
      ModelDataAccessor accessor(sourceModel(), source_row, source_parent, m_globalSourceModel);
      return m_ast->evaluate(accessor);
    }
    return true;
  }

  QAbstractItemModel *m = sourceModel();
  int cols = m->columnCount(source_parent);

  QStringList tokens = m_query.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  if (tokens.isEmpty())
    return true;

  QStringList colValues;
  for (int c = 0; c < cols; ++c) {
    QModelIndex idx = m->index(source_row, c, source_parent);
    colValues.append(m->data(idx, Qt::DisplayRole).toString());
    QVariant nameVar = m->data(idx, SourceModel::NameRole);
    if (nameVar.isValid()) {
      colValues.append(nameVar.toString());
    }
  }

  for (const QString &token : tokens) {
    bool tokenMatched = false;
    for (const QString &val : colValues) {
      if (val.contains(token, filterCaseSensitivity())) {
        tokenMatched = true;
        break;
      }
    }
    if (!tokenMatched) {
      return false;
    }
  }

  return true;
}

bool AdvancedFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
  QAbstractItemModel *m = sourceModel();

  // Try to cast to SourceModel or SessionModel to see if it supports
  // FavouriteRole
  int leftFav = -1;
  int rightFav = -1;

  if (qobject_cast<SourceModel *>(m)) {
    QVariant lVal = m->data(source_left, SourceModel::FavouriteRole);
    QVariant rVal = m->data(source_right, SourceModel::FavouriteRole);
    leftFav = lVal.isValid() ? lVal.toInt() : -1;
    rightFav = rVal.isValid() ? rVal.toInt() : -1;
  } else if (qobject_cast<SessionModel *>(m)) {
    QVariant lVal = m->data(source_left, SessionModel::FavouriteRole);
    QVariant rVal = m->data(source_right, SessionModel::FavouriteRole);
    leftFav = lVal.isValid() ? lVal.toInt() : -1;
    rightFav = rVal.isValid() ? rVal.toInt() : -1;
  }

  if (leftFav != rightFav) {
    if (sortOrder() == Qt::AscendingOrder) {
      return leftFav > rightFav;
    } else {
      return leftFav < rightFav;
      // is 'greater' -> return false
    }
  }

  return QSortFilterProxyModel::lessThan(source_left, source_right);
}

FollowingFilterProxyModel::FollowingFilterProxyModel(QObject *parent)
    : AdvancedFilterProxyModel(parent), m_tabType(FollowingTab) {}

void FollowingFilterProxyModel::setTabType(TabType type) {
  if (m_tabType == type)
    return;

  m_tabType = type;
  invalidateRowsFilter();
}

bool FollowingFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  QAbstractItemModel *m = sourceModel();
  if (m) {
    QModelIndex index = m->index(source_row, 0, source_parent);
    QVariant snoozeVar = m->data(index, SessionModel::SnoozeUntilRole);
    bool isSnoozed = false;
    if (snoozeVar.isValid()) {
      QDateTime snoozeUntil = snoozeVar.toDateTime();
      if (snoozeUntil.isValid() && snoozeUntil > QDateTime::currentDateTimeUtc()) {
        isSnoozed = true;
      }
    }

    if (m_tabType == FollowingTab && isSnoozed) {
      return false;
    } else if (m_tabType == SnoozedTab && !isSnoozed) {
      return false;
    }
  }

  return AdvancedFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}
