#include "sourcemodel.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDateTime>

SourceModel::SourceModel(QObject *parent) : QAbstractTableModel(parent) {
  loadSources();
}

int SourceModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_sources.size();
}

int SourceModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return ColCount;
}

QVariant SourceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sources.size() ||
      index.column() >= ColCount)
    return QVariant();

  const QJsonObject source = m_sources[index.row()].toObject();
  QString id = source.value(QStringLiteral("id")).toString();
  QString provider, owner, repo;

  QStringList parts = id.split(QLatin1Char('/'));
  if (parts.size() >= 4 && parts[0] == QStringLiteral("sources")) {
    provider = parts[1];
    owner = parts[2];
    repo = parts[3];
  } else if (parts.size() >= 3) {
    provider = parts[0];
    owner = parts[1];
    repo = parts[2];
  }

  if (role == Qt::DisplayRole) {
    if (index.column() == ColName) {
      // Reconstitute it first to drop the redundant sources/github/ prefix
      if (!provider.isEmpty() && !owner.isEmpty() && !repo.isEmpty()) {
        return owner + QLatin1Char('/') + repo;
      }

      QString name = source.value(QStringLiteral("name")).toString();
      if (!name.isEmpty() && name != id)
        return name;

      return id;
    } else if (index.column() == ColLastUsed) {
      QString valStr = source.value(QStringLiteral("local_lastUsed")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColSessionCount) {
      return source.value(QStringLiteral("local_sessionCount")).toInt();
    } else if (index.column() == ColHeat) {
      return source.value(QStringLiteral("local_heat")).toInt();
    } else if (index.column() == ColCreated) {
      QString valStr = source.value(QStringLiteral("createTime")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColUpdated) {
      QString valStr = source.value(QStringLiteral("updateTime")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    }
    return QVariant();
  }

  switch (role) {
  case NameRole:
    return source.value(QStringLiteral("name")).toString();
  case IdRole:
    return id;
  case RawDataRole:
    return source;
  default:
    return QVariant();
  }
}

QVariant SourceModel::headerData(int section, Qt::Orientation orientation,
                                 int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  if (section == ColName) {
    return QStringLiteral("Name");
  } else if (section == ColLastUsed) {
    return QStringLiteral("Last Used");
  } else if (section == ColSessionCount) {
    return QStringLiteral("Sessions");
  } else if (section == ColHeat) {
    return QStringLiteral("Heat");
  } else if (section == ColCreated) {
    return QStringLiteral("Created");
  } else if (section == ColUpdated) {
    return QStringLiteral("Updated");
  }
  return QVariant();
}

QHash<int, QByteArray> SourceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[IdRole] = "id";
  roles[RawDataRole] = "rawData";
  return roles;
}

void SourceModel::setSources(const QJsonArray &sources) {
  beginResetModel();
  QJsonArray newSources;
  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) id = source.value(QStringLiteral("name")).toString();

    for (int j = 0; j < m_sources.size(); ++j) {
      QString currentId = m_sources[j].toObject().value(QStringLiteral("id")).toString();
      if (currentId.isEmpty()) currentId = m_sources[j].toObject().value(QStringLiteral("name")).toString();
      if (currentId == id) {
        QJsonObject existing = m_sources[j].toObject();
        if (existing.contains(QStringLiteral("local_lastUsed"))) source[QStringLiteral("local_lastUsed")] = existing[QStringLiteral("local_lastUsed")];
        if (existing.contains(QStringLiteral("local_sessionCount"))) source[QStringLiteral("local_sessionCount")] = existing[QStringLiteral("local_sessionCount")];
        if (existing.contains(QStringLiteral("local_heat"))) source[QStringLiteral("local_heat")] = existing[QStringLiteral("local_heat")];
        if (existing.contains(QStringLiteral("local_sessionTimestamps"))) source[QStringLiteral("local_sessionTimestamps")] = existing[QStringLiteral("local_sessionTimestamps")];
        break;
      }
    }
    newSources.append(source);
  }
  m_sources = newSources;
  endResetModel();
  saveSources();
}
int SourceModel::addSources(const QJsonArray &sources) {
  int addedCount = 0;
  QJsonArray newSources;
  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) id = source.value(QStringLiteral("name")).toString();
    bool exists = false;
    for (int j = 0; j < m_sources.size(); ++j) {
      QString currentId = m_sources[j].toObject().value(QStringLiteral("id")).toString();
      if (currentId.isEmpty()) currentId = m_sources[j].toObject().value(QStringLiteral("name")).toString();
      if (currentId == id) {
        exists = true;
        QJsonObject existing = m_sources[j].toObject();
        if (existing.contains(QStringLiteral("local_lastUsed"))) source[QStringLiteral("local_lastUsed")] = existing[QStringLiteral("local_lastUsed")];
        if (existing.contains(QStringLiteral("local_sessionCount"))) source[QStringLiteral("local_sessionCount")] = existing[QStringLiteral("local_sessionCount")];
        if (existing.contains(QStringLiteral("local_heat"))) source[QStringLiteral("local_heat")] = existing[QStringLiteral("local_heat")];
        if (existing.contains(QStringLiteral("local_sessionTimestamps"))) source[QStringLiteral("local_sessionTimestamps")] = existing[QStringLiteral("local_sessionTimestamps")];
        m_sources[j] = source;
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
    saveSources();
  }
  return addedCount;
}
void SourceModel::loadSources() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/sources.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_sources = doc.array();
    file.close();
  }
}

void SourceModel::updateSource(const QJsonObject &sourceConst) {
  QJsonObject source = sourceConst;
  QString id = source.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) {
    // If id isn't there, maybe it's in name
    id = source.value(QStringLiteral("name")).toString();
  }

  if (id.isEmpty()) {
    return;
  }

  for (int i = 0; i < m_sources.size(); ++i) {
    QString currentId =
        m_sources[i].toObject().value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId =
          m_sources[i].toObject().value(QStringLiteral("name")).toString();
    }

    if (currentId == id) {
      QJsonObject existing = m_sources[i].toObject();
      if (existing.contains(QStringLiteral("local_lastUsed"))) source[QStringLiteral("local_lastUsed")] = existing[QStringLiteral("local_lastUsed")];
      if (existing.contains(QStringLiteral("local_sessionCount"))) source[QStringLiteral("local_sessionCount")] = existing[QStringLiteral("local_sessionCount")];
      if (existing.contains(QStringLiteral("local_heat"))) source[QStringLiteral("local_heat")] = existing[QStringLiteral("local_heat")];
      if (existing.contains(QStringLiteral("local_sessionTimestamps"))) source[QStringLiteral("local_sessionTimestamps")] = existing[QStringLiteral("local_sessionTimestamps")];
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      Q_EMIT dataChanged(index, index);
      saveSources();
      return;
    }
  }

  // Not found, append
  beginInsertRows(QModelIndex(), m_sources.size(), m_sources.size());
  m_sources.append(source);
  endInsertRows();
  saveSources();
}
void SourceModel::recordSessionCreated(const QString &sourceId) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    QString currentId = source.value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId = source.value(QStringLiteral("name")).toString();
    }

    if (currentId == sourceId) {
      source[QStringLiteral("local_lastUsed")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      int count = source.value(QStringLiteral("local_sessionCount")).toInt(0);
      source[QStringLiteral("local_sessionCount")] = count + 1;

      QJsonArray timestamps = source.value(QStringLiteral("local_sessionTimestamps")).toArray();
      timestamps.append(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

      qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
      qint64 sevenDaysAgo = now - (7 * 24 * 60 * 60);

      QJsonArray recentTimestamps;
      for (int j = 0; j < timestamps.size(); ++j) {
        qint64 ts = timestamps[j].toVariant().toLongLong();
        if (ts >= sevenDaysAgo) {
          recentTimestamps.append(ts);
        }
      }

      source[QStringLiteral("local_sessionTimestamps")] = recentTimestamps;
      source[QStringLiteral("local_heat")] = recentTimestamps.size();

      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}

void SourceModel::clear() {
  beginResetModel();
  m_sources = QJsonArray();
  endResetModel();
}

void SourceModel::saveSources() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(path + QStringLiteral("/sources.json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonDocument doc(m_sources);
    file.write(doc.toJson());
    file.close();
  }
}
