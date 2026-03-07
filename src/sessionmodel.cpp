#include "sessionmodel.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <KLocalizedString>

SessionModel::SessionModel(QObject *parent) : QAbstractTableModel(parent) {}

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
  if (!index.isValid() || index.row() >= m_sessions.size())
    return QVariant();

  const SessionData &session = m_sessions[index.row()];

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case ColTitle:
      return session.title;
    case ColSource:
      return session.source;
    case ColStatus:
      return session.status;
    case ColId:
      return session.id;
    default:
      return QVariant();
    }
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
  case StatusRole:
    return session.status;
  default:
    return QVariant();
  }
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case ColTitle:
      return i18n("Title");
    case ColSource:
      return i18n("Source");
    case ColStatus:
      return i18n("Status");
    case ColId:
      return i18n("ID");
    }
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
  m_rawSessions = sessions;
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
    data.status = obj.value(QStringLiteral("status")).toString();
    data.rawObject = obj;
    m_sessions.append(data);
    m_idToIndex[data.id] = i;
  }
  endResetModel();
}

int SessionModel::addSessions(const QJsonArray &sessions) {
  if (sessions.isEmpty()) {
    return 0;
  }

  QVector<QJsonObject> newSessions;
  for (int i = 0; i < sessions.size(); ++i) {
    QJsonObject obj = sessions[i].toObject();
    QString id = obj.value(QStringLiteral("id")).toString();
    if (!m_idToIndex.contains(id)) {
      newSessions.append(obj);
    }
  }

  if (newSessions.isEmpty()) {
    return 0;
  }

  beginInsertRows(QModelIndex(), m_sessions.size(), m_sessions.size() + newSessions.size() - 1);
  for (int i = 0; i < newSessions.size(); ++i) {
    QJsonObject obj = newSessions[i];
    SessionData data;
    data.id = obj.value(QStringLiteral("id")).toString();
    data.name = obj.value(QStringLiteral("name")).toString();
    data.title = obj.value(QStringLiteral("title")).toString();
    data.source = obj.value(QStringLiteral("sourceContext"))
                      .toObject()
                      .value(QStringLiteral("source"))
                      .toString();
    data.prompt = obj.value(QStringLiteral("prompt")).toString();
    data.status = obj.value(QStringLiteral("status")).toString();
    data.rawObject = obj;
    m_sessions.append(data);
    m_rawSessions.append(obj);
    m_idToIndex[data.id] = m_sessions.size() - 1;
  }
  endInsertRows();
  return newSessions.size();
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
  data.status = session.value(QStringLiteral("status")).toString();
  data.rawObject = session;
  m_sessions.insert(0, data);
  // Rebuild the index completely because inserting at 0 shifts all indices
  m_idToIndex.clear();
  for (int i = 0; i < m_sessions.size(); ++i) {
    m_idToIndex[m_sessions[i].id] = i;
  }
  endInsertRows();
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
    data.status = session.value(QStringLiteral("status")).toString();
    data.rawObject = session;
    m_sessions[i] = data;
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
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
  QFile file(path + QStringLiteral("/cached_all_sessions.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
      QJsonObject obj = doc.object();
      m_nextPageToken = obj.value(QStringLiteral("nextPageToken")).toString();
      setSessions(obj.value(QStringLiteral("sessions")).toArray());
    } else {
      setSessions(doc.array());
    }
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
  QFile file(path + QStringLiteral("/cached_all_sessions.json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonObject obj;
    obj[QStringLiteral("sessions")] = m_rawSessions;
    obj[QStringLiteral("nextPageToken")] = m_nextPageToken;
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
  }
}

void SessionModel::setNextPageToken(const QString &token) {
  m_nextPageToken = token;
}

QString SessionModel::nextPageToken() const {
  return m_nextPageToken;
}

void SessionModel::clearSessions() {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  m_rawSessions = QJsonArray();
  endResetModel();
}
