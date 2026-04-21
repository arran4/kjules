#include "sourcemodel.h"
#include <KLocalizedString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QStandardPaths>
#include <cmath>

static QString normalizeSourceId(const QString &id) {
  if (id.startsWith(QStringLiteral("sources/"))) {
    return id.mid(8);
  }
  return id;
}

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
    } else if (index.column() == ColFavourite) {
      return source.value(QStringLiteral("local_favourite")).toBool()
                 ? i18n("Yes")
                 : i18n("No");
    } else if (index.column() == ColLastUsed) {
      QString valStr =
          source.value(QStringLiteral("local_lastUsed")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColManagedSessions) {
      return source.value(QStringLiteral("local_sessionCount")).toInt();
    } else if (index.column() == ColHeat) {
      QJsonArray timestamps =
          source.value(QStringLiteral("local_sessionTimestamps")).toArray();
      qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
      double heat = 0.0;
      double halfLifeSecs = 7.0 * 24.0 * 60.0 * 60.0; // 7 days half-life
      double ln2 = 0.69314718056;

      for (int i = 0; i < timestamps.size(); ++i) {
        qint64 ts = timestamps[i].toVariant().toLongLong();
        if (ts <= now) {
          double age = static_cast<double>(now - ts);
          heat += std::exp(-(age / halfLifeSecs) * ln2);
        }
      }
      return std::round(heat * 10.0) / 10.0;
    } else if (index.column() == ColFirstSeen) {
      QString valStr =
          source.value(QStringLiteral("local_firstSeen")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColLastChanged) {
      QString valStr =
          source.value(QStringLiteral("local_lastChanged")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColDescription) {
      if (source.contains(QStringLiteral("description"))) {
        return source.value(QStringLiteral("description")).toString();
      }
      return source.value(QStringLiteral("github"))
          .toObject()
          .value(QStringLiteral("description"))
          .toString();
    } else if (index.column() == ColArchived) {
      if (source.contains(QStringLiteral("isArchived"))) {
        return source.value(QStringLiteral("isArchived")).toBool() ? i18n("Yes")
                                                                   : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github"))
                       .toObject()
                       .value(QStringLiteral("archived"))
                       .toBool()
                   ? i18n("Yes")
                   : i18n("No");
      }
    } else if (index.column() == ColFork) {
      if (source.contains(QStringLiteral("isFork"))) {
        return source.value(QStringLiteral("isFork")).toBool() ? i18n("Yes")
                                                               : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github"))
                       .toObject()
                       .value(QStringLiteral("fork"))
                       .toBool()
                   ? i18n("Yes")
                   : i18n("No");
      }
    } else if (index.column() == ColPrivate) {
      if (source.contains(QStringLiteral("isPrivate"))) {
        return source.value(QStringLiteral("isPrivate")).toBool() ? i18n("Yes")
                                                                  : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github"))
                       .toObject()
                       .value(QStringLiteral("private"))
                       .toBool()
                   ? i18n("Yes")
                   : i18n("No");
      }
    } else if (index.column() == ColLanguages) {
      if (source.contains(QStringLiteral("language"))) {
        return source.value(QStringLiteral("language")).toString();
      }
      return source.value(QStringLiteral("github"))
          .toObject()
          .value(QStringLiteral("language"))
          .toString();
    }
    return QVariant();
  } else if (role == Qt::DecorationRole) {
    if (index.column() == ColName) {
      if (source.value(QStringLiteral("isPrivate")).toBool()) {
        return QIcon::fromTheme(QStringLiteral("security-high"));
      }
    } else if (index.column() == ColFavourite) {
      if (source.value(QStringLiteral("local_favourite")).toBool()) {
        return QIcon::fromTheme(QStringLiteral("emblem-favorite"));
      }
    }
    return QVariant();
  } else
    switch (role) {
    case NameRole:
      return source.value(QStringLiteral("name")).toString();
    case IdRole:
      return id;
    case RawDataRole:
      return source;
    case FavouriteRole:
      return source.value(QStringLiteral("local_favourite")).toBool();
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
  } else if (section == ColFavourite) {
    return i18n("Favourite");
  } else if (section == ColLastUsed) {
    return QStringLiteral("Last Used");
  } else if (section == ColManagedSessions) {
    return QStringLiteral("Sessions");
  } else if (section == ColHeat) {
    return QStringLiteral("Heat");
  } else if (section == ColFirstSeen) {
    return QStringLiteral("First Seen");
  } else if (section == ColLastChanged) {
    return QStringLiteral("Last Changed");
  } else if (section == ColDescription) {
    return i18n("Description");
  } else if (section == ColArchived) {
    return i18n("Archived");
  } else if (section == ColFork) {
    return i18n("Fork");
  } else if (section == ColPrivate) {
    return i18n("Private");
  } else if (section == ColLanguages) {
    return i18n("Languages");
  }
  return QVariant();
}

QHash<int, QByteArray> SourceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[IdRole] = "id";
  roles[RawDataRole] = "rawData";
  roles[FavouriteRole] = "favourite";
  return roles;
}

void SourceModel::toggleFavourite(const QString &id) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    QString currentId = source.value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId = source.value(QStringLiteral("name")).toString();
    }

    if (currentId == id) {
      bool isFav = source.value(QStringLiteral("local_favourite")).toBool();
      source[QStringLiteral("local_favourite")] = !isFav;
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}

void SourceModel::setSources(const QJsonArray &sources) {
  beginResetModel();
  QJsonArray newSources;
  QHash<QString, bool> seenInNewSources;

  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
      id = source.value(QStringLiteral("name")).toString();

    QString normId = normalizeSourceId(id);
    if (seenInNewSources.contains(normId)) {
      continue;
    }
    seenInNewSources[normId] = true;

    for (int j = 0; j < m_sources.size(); ++j) {
      QString currentId =
          m_sources[j].toObject().value(QStringLiteral("id")).toString();
      if (currentId.isEmpty())
        currentId =
            m_sources[j].toObject().value(QStringLiteral("name")).toString();
      if (normalizeSourceId(currentId) == normId) {
        QJsonObject existing = m_sources[j].toObject();
        if (existing.contains(QStringLiteral("local_firstSeen")))
          source[QStringLiteral("local_firstSeen")] =
              existing[QStringLiteral("local_firstSeen")];
        if (existing.contains(QStringLiteral("local_lastChanged")))
          source[QStringLiteral("local_lastChanged")] =
              existing[QStringLiteral("local_lastChanged")];
        if (existing.contains(QStringLiteral("local_lastUsed")))
          source[QStringLiteral("local_lastUsed")] =
              existing[QStringLiteral("local_lastUsed")];
        if (existing.contains(QStringLiteral("local_sessionCount")))
          source[QStringLiteral("local_sessionCount")] =
              existing[QStringLiteral("local_sessionCount")];
        if (existing.contains(QStringLiteral("local_heat")))
          source[QStringLiteral("local_heat")] =
              existing[QStringLiteral("local_heat")];
        if (existing.contains(QStringLiteral("local_sessionTimestamps")))
          source[QStringLiteral("local_sessionTimestamps")] =
              existing[QStringLiteral("local_sessionTimestamps")];
        if (existing.contains(QStringLiteral("local_favourite")))
          source[QStringLiteral("local_favourite")] =
              existing[QStringLiteral("local_favourite")];
        break;
      }
    }
    if (!source.contains(QStringLiteral("local_firstSeen"))) {
      source[QStringLiteral("local_firstSeen")] =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    if (!source.contains(QStringLiteral("local_lastChanged"))) {
      source[QStringLiteral("local_lastChanged")] =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
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
  QHash<QString, bool> seenInNewSources;

  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
      id = source.value(QStringLiteral("name")).toString();

    QString normId = normalizeSourceId(id);
    if (seenInNewSources.contains(normId)) {
      continue;
    }

    bool exists = false;
    for (int j = 0; j < m_sources.size(); ++j) {
      QString currentId =
          m_sources[j].toObject().value(QStringLiteral("id")).toString();
      if (currentId.isEmpty())
        currentId =
            m_sources[j].toObject().value(QStringLiteral("name")).toString();
      if (normalizeSourceId(currentId) == normId) {
        exists = true;
        QJsonObject existing = m_sources[j].toObject();
        if (existing.contains(QStringLiteral("local_firstSeen")))
          source[QStringLiteral("local_firstSeen")] =
              existing[QStringLiteral("local_firstSeen")];
        if (existing.contains(QStringLiteral("local_lastChanged")))
          source[QStringLiteral("local_lastChanged")] =
              existing[QStringLiteral("local_lastChanged")];
        if (existing.contains(QStringLiteral("local_lastUsed")))
          source[QStringLiteral("local_lastUsed")] =
              existing[QStringLiteral("local_lastUsed")];
        if (existing.contains(QStringLiteral("local_sessionCount")))
          source[QStringLiteral("local_sessionCount")] =
              existing[QStringLiteral("local_sessionCount")];
        if (existing.contains(QStringLiteral("local_heat")))
          source[QStringLiteral("local_heat")] =
              existing[QStringLiteral("local_heat")];
        if (existing.contains(QStringLiteral("local_sessionTimestamps")))
          source[QStringLiteral("local_sessionTimestamps")] =
              existing[QStringLiteral("local_sessionTimestamps")];
        if (existing.contains(QStringLiteral("local_favourite")))
          source[QStringLiteral("local_favourite")] =
              existing[QStringLiteral("local_favourite")];
        m_sources[j] = source;
        break;
      }
    }
    if (!exists) {
      if (!source.contains(QStringLiteral("local_firstSeen"))) {
        source[QStringLiteral("local_firstSeen")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      }
      if (!source.contains(QStringLiteral("local_lastChanged"))) {
        source[QStringLiteral("local_lastChanged")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      }
      seenInNewSources[normId] = true;
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
    QJsonArray rawSources = doc.array();
    QJsonArray deduplicatedSources;
    QHash<QString, int> normalizedIdToIndex;
    bool modified = false;

    for (int i = 0; i < rawSources.size(); ++i) {
      QJsonObject source = rawSources[i].toObject();
      QString id = source.value(QStringLiteral("id")).toString();
      if (id.isEmpty()) {
        id = source.value(QStringLiteral("name")).toString();
      }
      QString normId = normalizeSourceId(id);

      if (normalizedIdToIndex.contains(normId)) {
        int existingIndex = normalizedIdToIndex[normId];
        QJsonObject existing = deduplicatedSources[existingIndex].toObject();

        int existingCount =
            existing.value(QStringLiteral("local_sessionCount")).toInt();
        int newCount =
            source.value(QStringLiteral("local_sessionCount")).toInt();
        if (newCount > existingCount) {
          existing[QStringLiteral("local_sessionCount")] = newCount;
        }

        if (source.contains(QStringLiteral("github")) &&
            !existing.contains(QStringLiteral("github"))) {
          existing[QStringLiteral("github")] =
              source.value(QStringLiteral("github"));
        }

        deduplicatedSources[existingIndex] = existing;
        modified = true;
      } else {
        normalizedIdToIndex[normId] = deduplicatedSources.size();
        deduplicatedSources.append(source);
      }
    }

    m_sources = deduplicatedSources;
    file.close();

    if (modified) {
      saveSources();
    }
  }
}

void SourceModel::updateSource(const QJsonObject &sourceConst) {
  QJsonObject source = sourceConst;
  QString id = source.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) {
    id = source.value(QStringLiteral("name")).toString();
  }

  if (id.isEmpty()) {
    return;
  }

  QString normId = normalizeSourceId(id);

  for (int i = 0; i < m_sources.size(); ++i) {
    QString currentId =
        m_sources[i].toObject().value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId =
          m_sources[i].toObject().value(QStringLiteral("name")).toString();
    }

    if (normalizeSourceId(currentId) == normId) {
      QJsonObject existing = m_sources[i].toObject();
      if (existing.contains(QStringLiteral("local_lastUsed")))
        source[QStringLiteral("local_lastUsed")] =
            existing[QStringLiteral("local_lastUsed")];
      if (existing.contains(QStringLiteral("local_sessionCount")))
        source[QStringLiteral("local_sessionCount")] =
            existing[QStringLiteral("local_sessionCount")];
      if (existing.contains(QStringLiteral("local_heat")))
        source[QStringLiteral("local_heat")] =
            existing[QStringLiteral("local_heat")];
      if (existing.contains(QStringLiteral("local_sessionTimestamps")))
        source[QStringLiteral("local_sessionTimestamps")] =
            existing[QStringLiteral("local_sessionTimestamps")];
      if (existing.contains(QStringLiteral("local_favourite")))
        source[QStringLiteral("local_favourite")] =
            existing[QStringLiteral("local_favourite")];
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      Q_EMIT dataChanged(index, index);
      saveSources();
      return;
    }
  }

  // Not found, append
  if (!source.contains(QStringLiteral("local_firstSeen"))) {
    source[QStringLiteral("local_firstSeen")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  }
  if (!source.contains(QStringLiteral("local_lastChanged"))) {
    source[QStringLiteral("local_lastChanged")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  }
  beginInsertRows(QModelIndex(), m_sources.size(), m_sources.size());
  m_sources.append(source);
  endInsertRows();
  saveSources();
}
void SourceModel::recordSessionCreated(const QString &sourceId) {
  QString normSourceId = normalizeSourceId(sourceId);
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    QString currentId = source.value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()) {
      currentId = source.value(QStringLiteral("name")).toString();
    }

    if (normalizeSourceId(currentId) == normSourceId) {
      source[QStringLiteral("local_lastUsed")] =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      source[QStringLiteral("local_lastChanged")] =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      int count = source.value(QStringLiteral("local_sessionCount")).toInt(0);
      source[QStringLiteral("local_sessionCount")] = count + 1;

      QJsonArray timestamps =
          source.value(QStringLiteral("local_sessionTimestamps")).toArray();
      timestamps.append(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

      qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
      qint64 thirtyDaysAgo = now - (30 * 24 * 60 * 60);

      QJsonArray recentTimestamps;
      for (int j = 0; j < timestamps.size(); ++j) {
        qint64 ts = timestamps[j].toVariant().toLongLong();
        // Keep timestamps up to 30 days to allow for meaningful decay
        if (ts >= thirtyDaysAgo) {
          recentTimestamps.append(ts);
        }
      }

      source[QStringLiteral("local_sessionTimestamps")] = recentTimestamps;

      // We calculate heat dynamically in data(), but update local_heat
      // so it's somewhat indicative if accessed elsewhere.
      double heat = 0.0;
      double halfLifeSecs = 7.0 * 24.0 * 60.0 * 60.0;
      double ln2 = 0.69314718056;
      for (int j = 0; j < recentTimestamps.size(); ++j) {
        qint64 ts = recentTimestamps[j].toVariant().toLongLong();
        if (ts <= now) {
          double age = static_cast<double>(now - ts);
          heat += std::exp(-(age / halfLifeSecs) * ln2);
        }
      }
      source[QStringLiteral("local_heat")] = std::round(heat * 10.0) / 10.0;

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

void SourceModel::recalculateStatsFromSessions(const QJsonArray &allSessions) {
  QHash<QString, int> sessionCounts;
  QHash<QString, QJsonArray> sessionTimestamps;
  QHash<QString, QString> lastUsedDates;

  for (int i = 0; i < allSessions.size(); ++i) {
    QJsonObject session = allSessions[i].toObject();

    // Determine the source from the session
    QString sourceId;
    if (session.contains(QStringLiteral("sourceContext"))) {
      sourceId = session.value(QStringLiteral("sourceContext"))
                     .toObject()
                     .value(QStringLiteral("source"))
                     .toString();
    }

    // A fallback if the session data doesn't perfectly match
    if (sourceId.isEmpty()) {
      sourceId = session.value(QStringLiteral("source")).toString();
    }

    if (sourceId.isEmpty())
      continue;

    // Normalize source ID
    QString normSourceId = normalizeSourceId(sourceId);

    QString updateTimeStr =
        session.value(QStringLiteral("updateTime")).toString();
    QString createTimeStr =
        session.value(QStringLiteral("createTime")).toString();
    QString timeStr = updateTimeStr.isEmpty() ? createTimeStr : updateTimeStr;

    qint64 ts = 0;
    if (!timeStr.isEmpty()) {
      QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
      if (dt.isValid()) {
        ts = dt.toSecsSinceEpoch();
      }
    }

    sessionCounts[normSourceId]++;
    if (ts > 0) {
      QJsonArray timestamps = sessionTimestamps[normSourceId];
      timestamps.append(ts);
      sessionTimestamps[normSourceId] = timestamps;

      if (!lastUsedDates.contains(normSourceId)) {
        lastUsedDates[normSourceId] = timeStr;
      } else {
        QDateTime currentLast =
            QDateTime::fromString(lastUsedDates[normSourceId], Qt::ISODate);
        QDateTime thisTime = QDateTime::fromString(timeStr, Qt::ISODate);
        if (thisTime > currentLast) {
          lastUsedDates[normSourceId] = timeStr;
        }
      }
    }
  }

  qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
  qint64 thirtyDaysAgo = now - (30 * 24 * 60 * 60);
  double halfLifeSecs = 7.0 * 24.0 * 60.0 * 60.0;
  double ln2 = 0.69314718056;

  bool changed = false;
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    QString id = source.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
      id = source.value(QStringLiteral("name")).toString();

    QString keyToUse = normalizeSourceId(id);

    int count = sessionCounts.value(keyToUse, 0);
    QJsonArray timestamps = sessionTimestamps.value(keyToUse, QJsonArray());
    QString lastUsed = lastUsedDates.value(keyToUse, QString());

    // Keep only recent timestamps
    QJsonArray recentTimestamps;
    for (int j = 0; j < timestamps.size(); ++j) {
      qint64 ts = timestamps[j].toVariant().toLongLong();
      if (ts >= thirtyDaysAgo) {
        recentTimestamps.append(ts);
      }
    }

    double heat = 0.0;
    for (int j = 0; j < recentTimestamps.size(); ++j) {
      qint64 ts = recentTimestamps[j].toVariant().toLongLong();
      if (ts <= now) {
        double age = static_cast<double>(now - ts);
        heat += std::exp(-(age / halfLifeSecs) * ln2);
      }
    }

    // Always update the source so it clears out sessions if they were deleted
    source[QStringLiteral("local_sessionCount")] = count;
    source[QStringLiteral("local_sessionTimestamps")] = recentTimestamps;
    source[QStringLiteral("local_heat")] = std::round(heat * 10.0) / 10.0;
    if (!lastUsed.isEmpty()) {
      source[QStringLiteral("local_lastUsed")] = lastUsed;
    }

    if (m_sources[i].toObject() != source) {
      m_sources[i] = source;
      changed = true;
    }
  }

  if (changed) {
    Q_EMIT dataChanged(createIndex(0, 0),
                       createIndex(m_sources.size() - 1, ColCount - 1));
    saveSources();
  }
}
