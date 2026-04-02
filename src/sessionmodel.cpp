#include "sessionmodel.h"
#include <KLocalizedString>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QJsonDocument>
#include <QStandardPaths>

SessionModel::SessionModel(const QString &cacheFileName, QObject *parent)
    : QAbstractTableModel(parent), m_cacheFileName(cacheFileName) {}

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
  data.updateTime = QDateTime::fromString(
      obj.value(QStringLiteral("updateTime")).toString(), Qt::ISODate);
  data.createTime = QDateTime::fromString(
      obj.value(QStringLiteral("createTime")).toString(), Qt::ISODate);

  data.hasChangeSet = false;
  QJsonArray outputs = obj.value(QStringLiteral("outputs")).toArray();
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outputObj = outputs[i].toObject();
    if (outputObj.contains(QStringLiteral("changeSet"))) {
      data.hasChangeSet = true;
    }
    if (outputObj.contains(QStringLiteral("pullRequest"))) {
      QJsonObject prObj =
          outputObj.value(QStringLiteral("pullRequest")).toObject();
      data.prUrl = prObj.value(QStringLiteral("url")).toString();
      if (!data.prUrl.isEmpty()) {
        int lastSlash = data.prUrl.lastIndexOf(QLatin1Char('/'));
        if (lastSlash != -1) {
          data.prNumber = QStringLiteral("#") + data.prUrl.mid(lastSlash + 1);
        }
      }
    }
  }

  if (obj.contains(QStringLiteral("githubPrData"))) {
    QJsonObject prData = obj.value(QStringLiteral("githubPrData")).toObject();
    data.prStatus = prData.value(QStringLiteral("state")).toString();
    data.prLabels = prData.value(QStringLiteral("labels")).toArray();
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
      return session.hasChangeSet ? i18n("has changes set") : QString();
    case ColPR:
      return session.prNumber;
    case ColPrStatus:
      return session.prStatus;
    case ColPrLabels: {
      QStringList labels;
      for (int i = 0; i < session.prLabels.size(); ++i) {
        labels << session.prLabels[i]
                      .toObject()
                      .value(QStringLiteral("name"))
                      .toString();
      }
      return labels.join(QStringLiteral(", "));
    }
    case ColUpdatedAt:
      return session.updateTime.toString(
          QLocale::system().dateFormat(QLocale::ShortFormat));
    case ColCreatedAt:
      return session.createTime.toString(
          QLocale::system().dateFormat(QLocale::ShortFormat));
    case ColOwner:
      return session.owner;
    case ColRepo:
      return session.repo;
    case ColId:
      return session.id;
    default:
      return QVariant();
    }
  } else if (role == Qt::ForegroundRole) {
    if (index.column() == ColPR && !session.prNumber.isEmpty()) {
      return QColor(Qt::blue);
    }
  } else if (role == Qt::FontRole) {
    if (index.column() == ColPR && !session.prNumber.isEmpty()) {
      QFont font;
      font.setUnderline(true);
      return font;
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
  case PrStatusRole:
    return session.prStatus;
  case PrLabelsRole:
    return session.prLabels;
  case ProviderRole:
    return session.provider;
  default:
    return QVariant();
  }
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
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
    case ColPrStatus:
      return i18n("PR Status");
    case ColPrLabels:
      return i18n("PR Labels");
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
    case ColLastRefreshed:
      return i18n("Last Refreshed");
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
  roles[LastRefreshedRole] = "lastRefreshed";
  return roles;
}

void SessionModel::setSessions(const QJsonArray &sessions) {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  m_sessions.reserve(sessions.size());
  for (int i = 0; i < sessions.size(); ++i) {
    QJsonObject obj = sessions[i].toObject();
    SessionData data = parseSessionData(obj);
    m_sessions.append(data);
    m_idToIndex[data.id] = i;
  }
  endResetModel();
  Q_EMIT sessionsLoadedOrUpdated();
}

int SessionModel::addSessions(const QJsonArray &sessions) {
  if (sessions.isEmpty()) {
    return 0;
  }

  QVector<QJsonObject> newSessions;
  for (int i = 0; i < sessions.size(); ++i) {
    QJsonObject obj = sessions[i].toObject();
    QString id = obj.value(QStringLiteral("id")).toString();
    if (m_idToIndex.contains(id)) {
      int row = m_idToIndex.value(id);
      SessionData data = parseSessionData(obj);
      data.id = id; // Ensure ID matches
      m_sessions[row] = data;
      Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
    } else {
      newSessions.append(obj);
    }
  }

  if (newSessions.isEmpty()) {
    return 0;
  }

  beginInsertRows(QModelIndex(), m_sessions.size(),
                  m_sessions.size() + newSessions.size() - 1);
  for (int i = 0; i < newSessions.size(); ++i) {
    QJsonObject obj = newSessions[i];
    SessionData data = parseSessionData(obj);
    m_sessions.append(data);
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
  QJsonObject sessionWithRefresh = session;
  sessionWithRefresh[QStringLiteral("lastRefreshed")] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QString id = sessionWithRefresh.value(QStringLiteral("id")).toString();
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData data = parseSessionData(sessionWithRefresh);
    data.id = id; // Ensure ID matches
    m_sessions[i] = data;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    return;
  }
  // If not found, add it
  addSession(sessionWithRefresh);
}

QJsonObject SessionModel::getSession(int row) const {
  if (row >= 0 && row < m_sessions.size()) {
    return m_sessions[row].rawObject;
  }
  return QJsonObject();
}

QJsonArray SessionModel::getAllSessions() const {
  QJsonArray arr;
  for (const SessionData &data : m_sessions) {
    arr.append(data.rawObject);
  }
  return arr;
}

void SessionModel::clear() {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  endResetModel();
  Q_EMIT sessionsLoadedOrUpdated();
}

void SessionModel::removeSession(int row) {
  if (row < 0 || row >= m_sessions.size())
    return;

  beginRemoveRows(QModelIndex(), row, row);
  m_sessions.removeAt(row);

  m_idToIndex.clear();
  for (int i = 0; i < m_sessions.size(); ++i) {
    m_idToIndex[m_sessions[i].id] = i;
  }
  endRemoveRows();
  saveSessions();
  Q_EMIT sessionsLoadedOrUpdated();
}

void SessionModel::loadSessions() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QLatin1Char('/') + m_cacheFileName);
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
  QFile file(path + QLatin1Char('/') + m_cacheFileName);
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonObject obj;

    QJsonArray sessionsArray;
    for (int i = 0; i < m_sessions.size(); ++i) {
      sessionsArray.append(m_sessions[i].rawObject);
    }

    obj[QStringLiteral("sessions")] = sessionsArray;
    obj[QStringLiteral("nextPageToken")] = m_nextPageToken;
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
  }
  Q_EMIT sessionsLoadedOrUpdated();
}

void SessionModel::setNextPageToken(const QString &token) {
  m_nextPageToken = token;
}

QString SessionModel::nextPageToken() const { return m_nextPageToken; }

void SessionModel::clearSessions() {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  endResetModel();
}

bool SessionModel::contains(const QString &id) const {
  return m_idToIndex.contains(id);
}
