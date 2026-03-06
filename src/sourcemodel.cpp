#include "sourcemodel.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

SourceModel::SourceModel(QObject *parent) : QAbstractListModel(parent) {
  loadCache();
}

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
  saveCache();
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
    saveCache();
  }
  return addedCount;
}

void SourceModel::loadCache() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (path.isEmpty())
    return;
  QFile file(path + QStringLiteral("/sources.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_sources = doc.array();
    file.close();
  }
}

void SourceModel::saveCache() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (path.isEmpty())
    return;
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
