#include "sourcemodel.h"
#include <QJsonObject>

SourceModel::SourceModel(QObject *parent) : QAbstractTableModel(parent) {}

int SourceModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_sources.size();
}

int SourceModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return 1; // Only 1 column, delegate draws everything
}

QVariant SourceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sources.size() ||
      index.column() >= 1)
    return QVariant();

  const QJsonObject source = m_sources[index.row()].toObject();

  switch (role) {
  case NameRole:
    return source.value(QStringLiteral("name")).toString();
  case IdRole:
    return source.value(QStringLiteral("id")).toString();
  case IsNewRole:
    // When the UI wants to know if this is new, it asks for IsNewRole
    return !m_knownSourceIds.contains(
        source.value(QStringLiteral("id")).toString());
  case TypeRole:
    return source.value(QStringLiteral("type")).toString();
  case DescriptionRole:
    return source.value(QStringLiteral("description")).toString();
  case LastUpdatedRole:
    return source.value(QStringLiteral("lastUpdated"))
        .toString(); // Assuming API provides this
  case Qt::DisplayRole:
    return source.value(QStringLiteral("name")).toString();
  default:
    return QVariant();
  }
}

QVariant SourceModel::headerData(int section, Qt::Orientation orientation,
                                 int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();

  if (orientation == Qt::Horizontal && section == 0) {
    return QStringLiteral("Source");
  }
  return QVariant();
}

QHash<int, QByteArray> SourceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[IdRole] = "id";
  roles[IsNewRole] = "isNew";
  roles[TypeRole] = "type";
  roles[DescriptionRole] = "description";
  roles[LastUpdatedRole] = "lastUpdated";
  return roles;
}

void SourceModel::setSources(const QJsonArray &sources) {
  // First time loading cache or initial, we just set known sources
  if (m_sources.isEmpty()) {
    for (const auto &s : sources) {
      m_knownSourceIds.append(
          s.toObject().value(QStringLiteral("id")).toString());
    }
    beginResetModel();
    m_sources = sources;
    endResetModel();
  } else {
    // Update existing items, add new items to avoid losing selection
    for (const auto &newSourceVal : sources) {
      QJsonObject newSource = newSourceVal.toObject();
      QString newId = newSource.value(QStringLiteral("id")).toString();

      bool found = false;
      for (int i = 0; i < m_sources.size(); ++i) {
        QJsonObject existingSource = m_sources[i].toObject();
        if (existingSource.value(QStringLiteral("id")).toString() == newId) {
          found = true;
          // Only emit dataChanged if something actually changed? We'll just
          // emit it to be safe
          m_sources[i] = newSourceVal;
          Q_EMIT dataChanged(index(i, 0), index(i, 0));
          break;
        }
      }

      if (!found) {
        beginInsertRows(QModelIndex(), m_sources.size(), m_sources.size());
        m_sources.append(newSourceVal);
        endInsertRows();
      }
    }
  }
}
