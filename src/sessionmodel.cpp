#include "sessionmodel.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <KLocalizedString>

SessionModel::SessionModel(QObject *parent) : QAbstractTableModel(parent) {}

SessionData parseSessionData(const QJsonObject &obj) {
  SessionData data;
  data.id = obj.value(QStringLiteral("id")).toString();
  data.name = obj.value(QStringLiteral("name")).toString();

  QString title = obj.value(QStringLiteral("title")).toString();
  QString prompt = obj.value(QStringLiteral("prompt")).toString();

  if (title.isEmpty()) {
    title = prompt;
  }
  title.replace(QLatin1Char('\n'), QLatin1Char(' '));
  if (title.length() > 50) {
    title = title.left(47) + QStringLiteral("...");
  }
  data.title = title;
  data.prompt = prompt;

  data.source = obj.value(QStringLiteral("sourceContext"))
                    .toObject()
                    .value(QStringLiteral("source"))
                    .toString();

  QStringList sourceParts = data.source.split(QLatin1Char('/'));
  if (sourceParts.size() >= 4 && sourceParts[0] == QStringLiteral("sources")) {
    data.provider = sourceParts[1];
    data.owner = sourceParts[2];
    data.repo = sourceParts[3];
  }

  data.state = obj.value(QStringLiteral("state")).toString();
  data.updateTime = QDateTime::fromString(obj.value(QStringLiteral("updateTime")).toString(), Qt::ISODate);
  data.createTime = QDateTime::fromString(obj.value(QStringLiteral("createTime")).toString(), Qt::ISODate);

  data.hasChangeSet = obj.contains(QStringLiteral("changeSet"));

  QJsonObject prObj = obj.value(QStringLiteral("pullRequest")).toObject();
  data.prUrl = prObj.value(QStringLiteral("url")).toString();
  if (!data.prUrl.isEmpty()) {
    int lastSlash = data.prUrl.lastIndexOf(QLatin1Char('/'));
    if (lastSlash != -1) {
      data.prNumber = QStringLiteral("#") + data.prUrl.mid(lastSlash + 1);
    }
  }

  data.rawObject = obj;
  return data;
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
  if (!index.isValid() || index.row() >= m_sessions.size())
    return QVariant();

  const SessionData &session = m_sessions[index.row()];

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case ColTitle:
      return session.title;
    case ColState:
      return session.state;
    case ColChangeSet:
      return session.hasChangeSet ? i18n("Yes") : QString();
    case ColPR:
      return session.prNumber;
    case ColUpdatedAt:
      return session.updateTime.toString(Qt::DefaultLocaleShortDate);
    case ColCreatedAt:
      return session.createTime.toString(Qt::DefaultLocaleShortDate);
    case ColOwner:
      return session.owner;
    case ColRepo:
      return session.repo;
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
  case StateRole:
    return session.state;
  case ChangeSetRole:
    return session.hasChangeSet;
  case PrUrlRole:
    return session.prUrl;
  case ProviderRole:
    return session.provider;
  default:
    return QVariant();
  }
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case ColTitle:
      return i18n("Title");
    case ColState:
      return i18n("State");
    case ColChangeSet:
      return i18n("Change Set");
    case ColPR:
      return i18n("PR");
    case ColUpdatedAt:
      return i18n("Updated At");
    case ColCreatedAt:
      return i18n("Created At");
    case ColOwner:
      return i18n("Owner");
    case ColRepo:
      return i18n("Repo");
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
  roles[StateRole] = "state";
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
    SessionData data = parseSessionData(obj);
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
    SessionData data = parseSessionData(obj);
    m_sessions.append(data);
    m_rawSessions.append(obj);
    m_idToIndex[data.id] = m_sessions.size() - 1;
  }
  endInsertRows();
  return newSessions.size();
}

void SessionModel::addSession(const QJsonObject &session) {
  beginInsertRows(QModelIndex(), 0, 0);
  SessionData data = parseSessionData(session);
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
    SessionData data = parseSessionData(session);
    data.id = id; // Ensure ID matches
    m_sessions[i] = data;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
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
