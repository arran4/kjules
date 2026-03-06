#include "sourcemodel.h"
#include <QJsonObject>

SourceModel::SourceModel(QObject *parent) : QAbstractListModel(parent) {}

int SourceModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_sources.size();
}

QVariant SourceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sources.size())
    return QVariant();

  const QJsonObject source = m_sources[index.row()].toObject();

  switch (role) {
  case NameRole:
    return source.value(QStringLiteral("name")).toString();
  case IdRole:
    return source.value(QStringLiteral("id")).toString();
  case Qt::DisplayRole:
    return source.value(QStringLiteral("name")).toString();
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> SourceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[IdRole] = "id";
  return roles;
}

void SourceModel::setSources(const QJsonArray &sources) {
  beginResetModel();
  m_sources = sources;
  endResetModel();
}

int SourceModel::addSources(const QJsonArray &sources) {
  int addedCount = 0;
  QJsonArray newSources;
  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    bool exists = false;
    for (int j = 0; j < m_sources.size(); ++j) {
      if (m_sources[j].toObject().value(QStringLiteral("id")).toString() ==
          id) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      newSources.append(source);
      addedCount++;
    }
  }

  if (addedCount > 0) {
    beginInsertRows(QModelIndex(), m_sources.size(),
                    m_sources.size() + addedCount - 1);
    for (int i = 0; i < newSources.size(); ++i) {
      m_sources.append(newSources[i]);
    }
    endInsertRows();
  }
  return addedCount;
}
