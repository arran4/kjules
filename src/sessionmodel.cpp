#include "sessionmodel.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

SessionModel::SessionModel(QObject *parent) : QAbstractTableModel(parent) {
  loadSessions();
}

int SessionModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;

  return m_sessions.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return ColCount;
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sessions.size() ||
      index.column() >= ColCount)
    return QVariant();

  const SessionData &session = m_sessions[index.row()];

  if (role == Qt::DisplayRole) {
    if (index.column() == ColName) {
      if (!session.title.isEmpty()) {
        return session.title;
      }
      if (!session.name.isEmpty()) {
        return session.name;
      }
      return session.id;
    }
    return QVariant();
  }

  switch (role) {
  case IdRole:
    return session.id;
  case NameRole:
    return session.name;
  case TitleRole:
    return session.title;
  case SourceRole:
    return session.source;
  case PromptRole:
    return session.prompt;
  default:
    return QVariant();
  }
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  if (section == ColName) {
    return QStringLiteral("Title");
  }
  return QVariant();
}

QHash<int, QByteArray> SessionModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[NameRole] = "name";
  roles[TitleRole] = "title";
  roles[SourceRole] = "source";
  roles[PromptRole] = "prompt";
  return roles;
}

void SessionModel::setSessions(const QJsonArray &sessions) {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  m_sessions.reserve(sessions.size());
  for (int i = 0; i < sessions.size(); ++i) {
    QJsonObject obj = sessions[i].toObject();
    SessionData data;
    data.id = obj.value(QStringLiteral("id")).toString();
    data.name = obj.value(QStringLiteral("name")).toString();
    data.title = obj.value(QStringLiteral("title")).toString();
    data.source = obj.value(QStringLiteral("sourceContext"))
                      .toObject()
                      .value(QStringLiteral("source"))
                      .toString();
    data.prompt = obj.value(QStringLiteral("prompt")).toString();
    data.rawObject = obj;
    m_sessions.append(data);
    m_idToIndex[data.id] = i;
  }
  endResetModel();
  saveSessions();
}

void SessionModel::addSession(const QJsonObject &session) {
  beginInsertRows(QModelIndex(), 0, 0);
  SessionData data;
  data.id = session.value(QStringLiteral("id")).toString();
  data.name = session.value(QStringLiteral("name")).toString();
  data.title = session.value(QStringLiteral("title")).toString();
  data.source = session.value(QStringLiteral("sourceContext"))
                    .toObject()
                    .value(QStringLiteral("source"))
                    .toString();
  data.prompt = session.value(QStringLiteral("prompt")).toString();
  data.rawObject = session;
  m_sessions.insert(0, data);
  // Rebuild the index completely because inserting at 0 shifts all indices
  m_idToIndex.clear();
  for (int i = 0; i < m_sessions.size(); ++i) {
    m_idToIndex[m_sessions[i].id] = i;
  }
  endInsertRows();
  saveSessions();
}

void SessionModel::updateSession(const QJsonObject &session) {
  QString id = session.value(QStringLiteral("id")).toString();
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData data;
    data.id = id;
    data.name = session.value(QStringLiteral("name")).toString();
    data.title = session.value(QStringLiteral("title")).toString();
    data.source = session.value(QStringLiteral("sourceContext"))
                      .toObject()
                      .value(QStringLiteral("source"))
                      .toString();
    data.prompt = session.value(QStringLiteral("prompt")).toString();
    data.rawObject = session;
    m_sessions[i] = data;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
    return;
  }
  // If not found, add it
  addSession(session);
}

QJsonObject SessionModel::getSession(int row) const {
  if (row >= 0 && row < m_sessions.size()) {
    return m_sessions[row].rawObject;
  }
  return QJsonObject();
}

void SessionModel::loadSessions() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/sessions.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray sessions = doc.array();
    beginResetModel();
    m_sessions.clear();
    m_idToIndex.clear();
    m_sessions.reserve(sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
      QJsonObject obj = sessions[i].toObject();
      SessionData data;
      data.id = obj.value(QStringLiteral("id")).toString();
      data.name = obj.value(QStringLiteral("name")).toString();
      data.title = obj.value(QStringLiteral("title")).toString();
      data.source = obj.value(QStringLiteral("sourceContext"))
                        .toObject()
                        .value(QStringLiteral("source"))
                        .toString();
      data.prompt = obj.value(QStringLiteral("prompt")).toString();
      data.rawObject = obj;
      m_sessions.append(data);
      m_idToIndex[data.id] = i;
    }
    endResetModel();
    file.close();
  }
}

void SessionModel::saveSessions() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(path + QStringLiteral("/sessions.json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonArray arr;
    for (const SessionData &data : qAsConst(m_sessions)) {
      arr.append(data.rawObject);
    }
    QJsonDocument doc(arr);
    file.write(doc.toJson());
    file.close();
  }
}
