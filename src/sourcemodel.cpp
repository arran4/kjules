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
  if (parent.isValid()) return 0;
  return m_sources.size();
}

QVariant SourceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sources.size()) return QVariant();

  const QJsonObject source = m_sources[index.row()].toObject();

  switch (role) {
    case NameRole:
      return source.value("name").toString();
    case IdRole:
      return source.value("id").toString();
    case OwnerRole:
      return source.value("githubRepo").toObject().value("owner").toString();
    case RepoRole:
      return source.value("githubRepo").toObject().value("repo").toString();
    case Qt::DisplayRole: {
      QJsonObject repo = source.value("githubRepo").toObject();
      QString owner = repo.value("owner").toString();
      QString repoName = repo.value("repo").toString();
      if (!owner.isEmpty() && !repoName.isEmpty()) {
        return owner + "/" + repoName;
      }
      return source.value("name").toString();
    }
    default:
      return QVariant();
  }
}

QHash<int, QByteArray> SourceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[IdRole] = "id";
  roles[OwnerRole] = "owner";
  roles[RepoRole] = "repo";
  return roles;
}

void SourceModel::setSources(const QJsonArray &sources) {
  beginResetModel();
  m_sources = sources;
  endResetModel();
  saveCache();
}

void SourceModel::addSources(const QJsonArray &sources) {
  if (sources.isEmpty()) return;
  beginInsertRows(QModelIndex(), m_sources.size(),
                  m_sources.size() + sources.size() - 1);
  for (const auto &s : sources) {
    m_sources.append(s);
  }
  endInsertRows();
  saveCache();
}

void SourceModel::loadCache() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QFile file(path + "/sources.json");
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_sources = doc.array();
    file.close();
  }
}

void SourceModel::saveCache() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(".");
  }
  QFile file(path + "/sources.json");
  if (file.open(QIODevice::WriteOnly)) {
    QJsonDocument doc(m_sources);
    file.write(doc.toJson());
    file.close();
  }
}
