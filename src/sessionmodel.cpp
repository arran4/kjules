#include "sessionmodel.h"
#include <QDebug>

SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent) {}

int SessionModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;

  return m_sessions.size();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_sessions.size())
    return QVariant();

  const SessionData &session = m_sessions[index.row()];

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
  case Qt::DisplayRole:
    return session.title;
  default:
    return QVariant();
  }
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

void SessionModel::clear() {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  endResetModel();
}
