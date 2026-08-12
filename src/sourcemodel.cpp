#include "sourcemodel.h"
#include "queuemodel.h"
#include "sessionmodel.h"
#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <cmath>

static QString customMigrationKey(const QJsonObject &source) {
  const QString owner = SourceModel::githubOwner(source);
  const QString repository = SourceModel::githubRepository(source);
  if (!owner.isEmpty() && !repository.isEmpty()) {
    return owner + QLatin1Char('/') + repository;
  }

  QString value = SourceModel::resourceName(source);
  if (value.startsWith(QStringLiteral("sources/"))) {
    value.remove(0, 8);
  }
  if (value.startsWith(QStringLiteral("github/"))) {
    value.remove(0, 7);
  }
  return value;
}

static bool isCustomReplacement(const QJsonObject &existing, const QJsonObject &replacement) {
  return existing.value(QStringLiteral("isCustom")).toBool() &&
         !replacement.value(QStringLiteral("isCustom")).toBool() && !customMigrationKey(existing).isEmpty() &&
         customMigrationKey(existing) == customMigrationKey(replacement);
}

QString SourceModel::resourceName(const QJsonObject &rawData) {
  const QString name = rawData.value(QStringLiteral("name")).toString();
  if (name.startsWith(QStringLiteral("sources/"))) {
    return name;
  }
  const QString legacyId = rawData.value(QStringLiteral("id")).toString();
  if (legacyId.startsWith(QStringLiteral("sources/"))) {
    return legacyId;
  }
  return name.isEmpty() ? legacyId : name;
}

void SourceModel::mergeLocalFields(const QString &id, const QJsonObject &existing, QJsonObject &source) {
  if (resourceName(existing) == id && existing.contains(QStringLiteral("isCustom")) &&
      !source.contains(QStringLiteral("githubRepo")) && !source.contains(QStringLiteral("github"))) {
    source[QStringLiteral("isCustom")] = existing.value(QStringLiteral("isCustom"));
  }

  for (auto it = existing.constBegin(); it != existing.constEnd(); ++it) {
    if (it.key().startsWith(QStringLiteral("local_"))) {
      if (it.key() != QStringLiteral("local_defaultBranches")) {
        source[it.key()] = it.value();
      }
    }
  }

  QStringList localDefaults;
  if (existing.contains(QStringLiteral("local_defaultBranches"))) {
    QJsonArray arr = existing.value(QStringLiteral("local_defaultBranches")).toArray();
    for (const QJsonValue &v : arr) {
      localDefaults.append(v.toString());
    }
  } else {
    QString oldApiBranch = extractApiDefaultBranch(existing);
    if (!oldApiBranch.isEmpty()) {
      localDefaults.append(oldApiBranch);
    }
  }

  QString newApiBranch = extractApiDefaultBranch(source);
  if (!newApiBranch.isEmpty() && !localDefaults.contains(newApiBranch)) {
    localDefaults.append(newApiBranch);
  }

  if (!localDefaults.isEmpty()) {
    QJsonArray arr;
    for (const QString &b : localDefaults) {
      arr.append(b);
    }
    source[QStringLiteral("local_defaultBranches")] = arr;
  }
}

QString SourceModel::githubOwner(const QJsonObject &rawData) {
  const QJsonObject githubRepo = rawData.value(QStringLiteral("githubRepo")).toObject();
  const QString apiOwner = githubRepo.value(QStringLiteral("owner")).toString();
  if (!apiOwner.isEmpty()) {
    return apiOwner;
  }

  const QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  const QJsonValue owner = github.value(QStringLiteral("owner"));
  return owner.isObject() ? owner.toObject().value(QStringLiteral("login")).toString() : owner.toString();
}

QString SourceModel::githubRepository(const QJsonObject &rawData) {
  const QString apiRepository =
      rawData.value(QStringLiteral("githubRepo")).toObject().value(QStringLiteral("repo")).toString();
  if (!apiRepository.isEmpty()) {
    return apiRepository;
  }
  return rawData.value(QStringLiteral("github")).toObject().value(QStringLiteral("name")).toString();
}

QString SourceModel::repositoryUrl(const QJsonObject &rawData) {
  const QString apiUrl =
      rawData.value(QStringLiteral("github")).toObject().value(QStringLiteral("html_url")).toString();
  if (!apiUrl.isEmpty()) {
    return apiUrl;
  }
  const QString owner = githubOwner(rawData);
  const QString repository = githubRepository(rawData);
  if (!owner.isEmpty() && !repository.isEmpty()) {
    return QStringLiteral("https://github.com/%1/%2").arg(owner, repository);
  }
  return {};
}

SourceModel::SourceModel(QObject *parent, StorageMode storageMode)
    : QAbstractTableModel(parent), m_storageMode(storageMode) {
  if (m_storageMode == StorageMode::Persistent) {
    loadSources();
  }
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
  if (!index.isValid() || index.row() >= m_sources.size() || index.column() >= ColCount)
    return QVariant();

  const QJsonObject source = m_sources[index.row()].toObject();
  const QString id = resourceName(source);
  QString provider, owner, repo;

  owner = githubOwner(source);
  repo = githubRepository(source);
  if (!owner.isEmpty() && !repo.isEmpty()) {
    provider = QStringLiteral("github");
  } else {
    // Parsing is intentionally limited to presentation of legacy/custom rows.
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() >= 4 && parts[0] == QStringLiteral("sources")) {
      provider = parts[1];
      owner = parts[2];
      repo = parts[3];
    } else if (parts.size() >= 3) {
      provider = parts[0];
      owner = parts[1];
      repo = parts[2];
    }
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
    } else if (index.column() == ColManagedSessions) {
      return source.value(QStringLiteral("local_sessionCount")).toInt();
    } else if (index.column() == ColHeat) {
      QJsonArray timestamps = source.value(QStringLiteral("local_sessionTimestamps")).toArray();
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
      QString valStr = source.value(QStringLiteral("local_firstSeen")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColLastChanged) {
      QString valStr = source.value(QStringLiteral("local_lastChanged")).toString();
      if (!valStr.isEmpty()) {
        return QDateTime::fromString(valStr, Qt::ISODate);
      }
      return QVariant();
    } else if (index.column() == ColDescription) {
      if (source.contains(QStringLiteral("description"))) {
        return source.value(QStringLiteral("description")).toString();
      }
      return source.value(QStringLiteral("github")).toObject().value(QStringLiteral("description")).toString();
    } else if (index.column() == ColArchived) {
      if (source.contains(QStringLiteral("isArchived"))) {
        return source.value(QStringLiteral("isArchived")).toBool() ? i18n("Yes") : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github")).toObject().value(QStringLiteral("archived")).toBool()
                   ? i18n("Yes")
                   : i18n("No");
      }
    } else if (index.column() == ColFork) {
      if (source.contains(QStringLiteral("isFork"))) {
        return source.value(QStringLiteral("isFork")).toBool() ? i18n("Yes") : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github")).toObject().value(QStringLiteral("fork")).toBool() ? i18n("Yes")
                                                                                                        : i18n("No");
      }
    } else if (index.column() == ColPrivate) {
      if (source.contains(QStringLiteral("isPrivate"))) {
        return source.value(QStringLiteral("isPrivate")).toBool() ? i18n("Yes") : i18n("No");
      } else if (source.contains(QStringLiteral("github"))) {
        return source.value(QStringLiteral("github")).toObject().value(QStringLiteral("private")).toBool() ? i18n("Yes")
                                                                                                           : i18n("No");
      }
    } else if (index.column() == ColLanguages) {
      if (source.contains(QStringLiteral("language"))) {
        return source.value(QStringLiteral("language")).toString();
      }
      return source.value(QStringLiteral("github")).toObject().value(QStringLiteral("language")).toString();
    } else if (index.column() == ColQueueStatus) {
      int blockedCount = source.value(QStringLiteral("local_blockedCount")).toInt();
      int pendingCount = source.value(QStringLiteral("local_pendingCount")).toInt();
      int inProgressCount = source.value(QStringLiteral("local_inProgressCount")).toInt();

      QStringList parts;
      if (blockedCount > 0)
        parts.append(i18n("%1 Blocked", blockedCount));
      if (inProgressCount > 0)
        parts.append(i18n("%1 In Progress", inProgressCount));
      if (pendingCount > 0)
        parts.append(i18n("%1 Pending", pendingCount));

      if (parts.isEmpty())
        return QStringLiteral("-");
      return parts.join(QStringLiteral(", "));
    }
    return QVariant();
  } else if (role == Qt::DecorationRole) {
    if (index.column() == ColName) {
      QJsonValue favVal = source.value(QStringLiteral("local_favourite"));
      bool isFav = favVal.isDouble() && favVal.toInt() > 0;
      bool isPriv = source.value(QStringLiteral("isPrivate")).toBool();

      if (isFav && isPriv) {
        QIcon favIcon = QIcon::fromTheme(QStringLiteral("emblem-favorite"));
        QIcon privIcon = QIcon::fromTheme(QStringLiteral("security-high"));

        int size = 16;
        QPixmap combined(size * 2 + 2, size);
        combined.fill(Qt::transparent);

        QPainter p(&combined);
        p.drawPixmap(0, 0, favIcon.pixmap(size, size));
        p.drawPixmap(size + 2, 0, privIcon.pixmap(size, size));
        p.end();

        return QIcon(combined);
      } else if (isFav) {
        return QIcon::fromTheme(QStringLiteral("emblem-favorite"));
      } else if (isPriv) {
        return QIcon::fromTheme(QStringLiteral("security-high"));
      }
    }
    return QVariant();
  } else
    switch (role) {
    case NameRole: {
      if (!provider.isEmpty() && !owner.isEmpty() && !repo.isEmpty()) {
        return owner + QLatin1Char('/') + repo;
      }
      QString name = source.value(QStringLiteral("name")).toString();
      if (!name.isEmpty() && name != id)
        return name;
      QString normId = id;
      if (normId.startsWith(QStringLiteral("sources/"))) {
        normId = normId.mid(8);
      }
      if (normId.startsWith(QStringLiteral("github/"))) {
        normId = normId.mid(7);
      }
      return normId;
    }
    case IdRole:
      return id;
    case RawDataRole:
      return source;
    case FavouriteRole: {
      QJsonValue favVal = source.value(QStringLiteral("local_favourite"));
      if (favVal.isBool()) {
        return favVal.toBool() ? QVariant(1) : QVariant();
      } else if (favVal.isDouble()) {
        return QVariant(favVal.toInt());
      }
      return QVariant();
    }
    default:
      return QVariant();
    }
}

QVariant SourceModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  if (section == ColName) {
    return QStringLiteral("Name");
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
  } else if (section == ColQueueStatus) {
    return i18n("Queue Status");
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
    const QString currentId = resourceName(source);

    if (currentId == id) {
      QJsonValue favVal = source.value(QStringLiteral("local_favourite"));
      int currentRank = 0;
      if (favVal.isBool()) {
        currentRank = favVal.toBool() ? 1 : 0;
      } else if (favVal.isDouble()) {
        currentRank = favVal.toInt();
      }
      if (currentRank > 0) {
        source.remove(QStringLiteral("local_favourite"));
      } else {
        source[QStringLiteral("local_favourite")] = 1;
      }
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}

void SourceModel::setFavouriteRank(const QString &id, int rank) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    const QString currentId = resourceName(source);
    if (currentId == id) {
      source[QStringLiteral("local_favourite")] = rank;
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}

void SourceModel::increaseFavouriteRank(const QString &id) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    const QString currentId = resourceName(source);
    if (currentId == id) {
      QJsonValue favVal = source.value(QStringLiteral("local_favourite"));
      int currentRank = 0;
      if (favVal.isBool()) {
        currentRank = favVal.toBool() ? 1 : 0;
      } else if (favVal.isDouble()) {
        currentRank = favVal.toInt();
      }
      source[QStringLiteral("local_favourite")] = currentRank + 1;
      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}

void SourceModel::decreaseFavouriteRank(const QString &id) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    const QString currentId = resourceName(source);
    if (currentId == id) {
      QJsonValue favVal = source.value(QStringLiteral("local_favourite"));
      int currentRank = 0;
      if (favVal.isBool()) {
        currentRank = favVal.toBool() ? 1 : 0;
      } else if (favVal.isDouble()) {
        currentRank = favVal.toInt();
      }
      int newRank = currentRank - 1;
      if (newRank <= 0) {
        source.remove(QStringLiteral("local_favourite"));
      } else {
        source[QStringLiteral("local_favourite")] = newRank;
      }
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
  QJsonArray customSources;
  for (const QJsonValue &value : m_sources) {
    const QJsonObject existing = value.toObject();
    if (existing.value(QStringLiteral("isCustom")).toBool()) {
      customSources.append(existing);
    }
  }

  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();

    const QString id = resourceName(source);
    if (seenInNewSources.contains(id)) {
      continue;
    }
    seenInNewSources[id] = true;

    for (int j = 0; j < m_sources.size(); ++j) {
      const QJsonObject existing = m_sources[j].toObject();
      if (resourceName(existing) == id || isCustomReplacement(existing, source)) {
        mergeLocalFields(id, existing, source);
        break;
      }
    }
    if (!source.contains(QStringLiteral("local_firstSeen"))) {
      source[QStringLiteral("local_firstSeen")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    if (!source.contains(QStringLiteral("local_lastChanged"))) {
      source[QStringLiteral("local_lastChanged")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    newSources.append(source);
  }
  for (const QJsonValue &value : customSources) {
    const QJsonObject custom = value.toObject();
    const QString customId = resourceName(custom);
    bool replaced = false;
    for (const QJsonValue &newValue : newSources) {
      if (isCustomReplacement(custom, newValue.toObject())) {
        replaced = true;
        break;
      }
    }
    if (!seenInNewSources.contains(customId) && !replaced) {
      newSources.append(custom);
    }
  }
  m_sources = newSources;
  endResetModel();
  saveSources();
}
int SourceModel::addSources(const QJsonArray &sources) {
  int addedCount = 0;
  bool updatedExisting = false;
  QJsonArray newSources;
  QHash<QString, bool> seenInNewSources;

  for (int i = 0; i < sources.size(); ++i) {
    QJsonObject source = sources[i].toObject();
    const QString id = resourceName(source);
    if (seenInNewSources.contains(id)) {
      continue;
    }
    seenInNewSources[id] = true;

    bool exists = false;
    for (int j = 0; j < m_sources.size(); ++j) {
      const QJsonObject existingObject = m_sources[j].toObject();
      if (resourceName(existingObject) == id || isCustomReplacement(existingObject, source)) {
        exists = true;
        mergeLocalFields(id, existingObject, source);
        m_sources[j] = source;
        updatedExisting = true;
        Q_EMIT dataChanged(index(j, 0), index(j, ColCount - 1));
        break;
      }
    }
    if (!exists) {
      if (!source.contains(QStringLiteral("local_firstSeen"))) {
        source[QStringLiteral("local_firstSeen")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      }
      if (!source.contains(QStringLiteral("local_lastChanged"))) {
        source[QStringLiteral("local_lastChanged")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      }
      newSources.append(source);
      addedCount++;
    }
  }

  if (addedCount > 0) {
    beginInsertRows(QModelIndex(), m_sources.size(), m_sources.size() + addedCount - 1);
    for (int i = 0; i < newSources.size(); ++i) {
      m_sources.append(newSources[i]);
    }
    endInsertRows();
    saveSources();
  } else if (updatedExisting) {
    saveSources();
  }
  return addedCount;
}
void SourceModel::loadSources() {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/sources.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray rawSources = doc.array();
    QJsonArray deduplicatedSources;
    QHash<QString, int> resourceNameToIndex;
    bool modified = false;

    for (int i = 0; i < rawSources.size(); ++i) {
      QJsonObject source = rawSources[i].toObject();
      const QString id = resourceName(source);

      if (resourceNameToIndex.contains(id)) {
        int existingIndex = resourceNameToIndex[id];
        QJsonObject existing = deduplicatedSources[existingIndex].toObject();

        int existingCount = existing.value(QStringLiteral("local_sessionCount")).toInt();
        int newCount = source.value(QStringLiteral("local_sessionCount")).toInt();
        if (newCount > existingCount) {
          existing[QStringLiteral("local_sessionCount")] = newCount;
        }

        if (source.contains(QStringLiteral("github")) && !existing.contains(QStringLiteral("github"))) {
          existing[QStringLiteral("github")] = source.value(QStringLiteral("github"));
        }

        deduplicatedSources[existingIndex] = existing;
        modified = true;
      } else {
        resourceNameToIndex[id] = deduplicatedSources.size();
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
  const QString id = resourceName(source);

  if (id.isEmpty()) {
    return;
  }

  for (int i = 0; i < m_sources.size(); ++i) {
    const QJsonObject existing = m_sources[i].toObject();
    if (resourceName(existing) == id || isCustomReplacement(existing, source)) {
      mergeLocalFields(id, existing, source);

      m_sources[i] = source;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }

  // Not found, append
  if (!source.contains(QStringLiteral("local_firstSeen"))) {
    source[QStringLiteral("local_firstSeen")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  }
  if (!source.contains(QStringLiteral("local_lastChanged"))) {
    source[QStringLiteral("local_lastChanged")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  }
  beginInsertRows(QModelIndex(), m_sources.size(), m_sources.size());
  m_sources.append(source);
  endInsertRows();
  saveSources();
}

void SourceModel::removeSource(const QString &id) {
  for (int i = 0; i < m_sources.size(); ++i) {
    if (resourceName(m_sources[i].toObject()) == id) {
      beginRemoveRows(QModelIndex(), i, i);
      m_sources.removeAt(i);
      endRemoveRows();
      saveSources();
      return;
    }
  }
}
void SourceModel::recordSessionCreated(const QString &sourceId) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    if (resourceName(source) == sourceId) {
      source[QStringLiteral("local_lastUsed")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      source[QStringLiteral("local_lastChanged")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      int count = source.value(QStringLiteral("local_sessionCount")).toInt(0);
      source[QStringLiteral("local_sessionCount")] = count + 1;

      QJsonArray timestamps = source.value(QStringLiteral("local_sessionTimestamps")).toArray();
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
  if (m_storageMode == StorageMode::InMemory) {
    return;
  }

  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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
      sourceId = session.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
    }

    // A fallback if the session data doesn't perfectly match
    if (sourceId.isEmpty()) {
      sourceId = session.value(QStringLiteral("source")).toString();
    }

    if (sourceId.isEmpty())
      continue;

    QString updateTimeStr = session.value(QStringLiteral("updateTime")).toString();
    QString createTimeStr = session.value(QStringLiteral("createTime")).toString();
    QString timeStr = updateTimeStr.isEmpty() ? createTimeStr : updateTimeStr;

    qint64 ts = 0;
    if (!timeStr.isEmpty()) {
      QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
      if (dt.isValid()) {
        ts = dt.toSecsSinceEpoch();
      }
    }

    sessionCounts[sourceId]++;
    if (ts > 0) {
      QJsonArray timestamps = sessionTimestamps[sourceId];
      timestamps.append(ts);
      sessionTimestamps[sourceId] = timestamps;

      if (!lastUsedDates.contains(sourceId)) {
        lastUsedDates[sourceId] = timeStr;
      } else {
        QDateTime currentLast = QDateTime::fromString(lastUsedDates[sourceId], Qt::ISODate);
        QDateTime thisTime = QDateTime::fromString(timeStr, Qt::ISODate);
        if (thisTime > currentLast) {
          lastUsedDates[sourceId] = timeStr;
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
    const QString keyToUse = resourceName(source);

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
    Q_EMIT dataChanged(createIndex(0, 0), createIndex(m_sources.size() - 1, ColCount - 1));
    saveSources();
  }
}

void SourceModel::recalculateQueueStats(QueueModel *queueModel, SessionModel *sessionModel) {
  if (!queueModel || !sessionModel)
    return;

  QHash<QString, int> blockedCounts;
  QHash<QString, int> pendingCounts;
  QHash<QString, int> inProgressCounts;

  for (int i = 0; i < queueModel->rowCount(); ++i) {
    QueueItem item = queueModel->getItem(i);
    QString source =
        item.requestData.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
    if (source.isEmpty()) {
      source = item.requestData.value(QStringLiteral("source")).toString();
    }
    if (source.isEmpty())
      continue;

    if (item.isBlocked) {
      blockedCounts[source]++;
    } else {
      pendingCounts[source]++;
    }
  }

  for (int i = 0; i < sessionModel->rowCount(); ++i) {
    QJsonObject session = sessionModel->getSession(i);
    QString state = session.value(QStringLiteral("state")).toString();
    if (state == QStringLiteral("IN_PROGRESS")) {
      QString sourceId;
      if (session.contains(QStringLiteral("sourceContext"))) {
        sourceId = session.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
      }
      if (sourceId.isEmpty()) {
        sourceId = session.value(QStringLiteral("source")).toString();
      }
      if (!sourceId.isEmpty()) {
        inProgressCounts[sourceId]++;
      }
    }
  }

  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject source = m_sources[i].toObject();
    const QString sourceId = resourceName(source);

    int newBlocked = blockedCounts.value(sourceId, 0);
    int newPending = pendingCounts.value(sourceId, 0);
    int newInProgress = inProgressCounts.value(sourceId, 0);

    if (source.value(QStringLiteral("local_blockedCount")).toInt() != newBlocked ||
        source.value(QStringLiteral("local_pendingCount")).toInt() != newPending ||
        source.value(QStringLiteral("local_inProgressCount")).toInt() != newInProgress) {

      source[QStringLiteral("local_blockedCount")] = newBlocked;
      source[QStringLiteral("local_pendingCount")] = newPending;
      source[QStringLiteral("local_inProgressCount")] = newInProgress;
      m_sources[i] = source;
      QModelIndex idx = index(i, ColQueueStatus);
      Q_EMIT dataChanged(idx, idx);
    }
  }
}

QString SourceModel::extractApiDefaultBranch(const QJsonObject &rawData) {
  QJsonObject githubRepo = rawData.value(QStringLiteral("githubRepo")).toObject();
  if (githubRepo.contains(QStringLiteral("defaultBranch"))) {
    QJsonObject db = githubRepo.value(QStringLiteral("defaultBranch")).toObject();
    if (db.contains(QStringLiteral("displayName"))) {
      return db.value(QStringLiteral("displayName")).toString();
    }
  }

  if (rawData.contains(QStringLiteral("defaultBranch"))) {
    return rawData.value(QStringLiteral("defaultBranch")).toString();
  }
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  if (github.contains(QStringLiteral("default_branch"))) {
    return github.value(QStringLiteral("default_branch")).toString();
  }
  return QString();
}

void SourceModel::setDefaultBranches(const QString &id, const QStringList &branches) {
  for (int i = 0; i < m_sources.size(); ++i) {
    QJsonObject obj = m_sources[i].toObject();
    if (resourceName(obj) == id) {
      QJsonArray arr;
      for (const QString &b : branches) {
        arr.append(b);
      }
      obj[QStringLiteral("local_defaultBranches")] = arr;
      m_sources[i] = obj;
      QModelIndex index = createIndex(i, 0);
      QModelIndex lastColIndex = createIndex(i, ColCount - 1);
      Q_EMIT dataChanged(index, lastColIndex);
      saveSources();
      return;
    }
  }
}
