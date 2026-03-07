#include "sourcemodel.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QStandardPaths>

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
  m_sources = sources;
  endResetModel();
  saveSources();
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
        // Optionally update the existing source here.
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

