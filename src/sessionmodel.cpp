#include "sessionmodel.h"
#include <KLocalizedString>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QIcon>
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
  if (title.length() > 1024) {
    title = title.left(47) + QStringLiteral("...");
  }
  data.title = title;
  data.prompt = prompt;

  data.source = obj.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();

  QStringList sourceParts = data.source.split(QLatin1Char('/'));
  if (sourceParts.size() >= 4 && sourceParts[0] == QStringLiteral("sources")) {
    data.provider = sourceParts[1];
    data.owner = sourceParts[2];
    data.repo = sourceParts[3];
  }

  data.state = obj.value(QStringLiteral("state")).toString();
  data.updateTime = QDateTime::fromString(obj.value(QStringLiteral("updateTime")).toString(), Qt::ISODate);
  data.createTime = QDateTime::fromString(obj.value(QStringLiteral("createTime")).toString(), Qt::ISODate);
  data.lastRefreshed = QDateTime::fromString(obj.value(QStringLiteral("lastRefreshed")).toString(), Qt::ISODate);

  QJsonValue favVal = obj.value(QStringLiteral("local_favourite"));
  if (favVal.isDouble()) {
    data.favouriteRank = favVal.toInt();
  } else {
    data.favouriteRank = std::nullopt;
  }

  QJsonValue snoozeVal = obj.value(QStringLiteral("local_snooze_until"));
  if (!snoozeVal.isUndefined() && snoozeVal.isString()) {
    data.snoozeUntil = QDateTime::fromString(snoozeVal.toString(), Qt::ISODate);
  }

  data.hasChangeSet = false;
  QJsonArray outputs = obj.value(QStringLiteral("outputs")).toArray();
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outputObj = outputs[i].toObject();
    if (outputObj.contains(QStringLiteral("changeSet"))) {
      data.hasChangeSet = true;
    }
    if (outputObj.contains(QStringLiteral("pullRequest"))) {
      QJsonObject prObj = outputObj.value(QStringLiteral("pullRequest")).toObject();
      data.prUrl = prObj.value(QStringLiteral("url")).toString();
      if (!data.prUrl.isEmpty() && data.prUrl != QLatin1StringView("undefined")) {
        int lastSlash = data.prUrl.lastIndexOf(QLatin1Char('/'));
        if (lastSlash != -1) {
          data.prNumber = QStringLiteral("#") + data.prUrl.mid(lastSlash + 1);
        }
      } else {
        data.prUrl.clear();
      }
    }
  }

  if (obj.contains(QStringLiteral("githubPrInfo"))) {
    QJsonObject prInfo = obj.value(QStringLiteral("githubPrInfo")).toObject();
    data.prStatus = prInfo.value(QStringLiteral("state")).toString();
    if (prInfo.value(QStringLiteral("merged_at")).isString()) {
      data.prStatus = QStringLiteral("merged");
    }
    QJsonArray labelsArr = prInfo.value(QStringLiteral("labels")).toArray();
    for (int i = 0; i < labelsArr.size(); ++i) {
      data.prLabels.append(labelsArr[i].toObject().value(QStringLiteral("name")).toString());
    }
  }

  QJsonValue refreshVal = obj.value(QStringLiteral("local_refreshInterval"));
  if (refreshVal.isDouble()) {
    data.refreshInterval = refreshVal.toInt();
  } else {
    data.refreshInterval = std::nullopt;
  }

  data.rawObject = obj;
  data.hasUnreadChanges = false;
  return data;
}

static void mergeClientFields(const SessionData &existing, QJsonObject &incomingObj, SessionData &data,
                              bool isSuccessfulRefresh) {
  // 1. local_favourite
  if (existing.favouriteRank.has_value()) {
    data.favouriteRank = existing.favouriteRank;
    incomingObj[QStringLiteral("local_favourite")] = existing.favouriteRank.value();
    data.rawObject[QStringLiteral("local_favourite")] = existing.favouriteRank.value();
  } else {
    data.favouriteRank = std::nullopt;
    incomingObj.remove(QStringLiteral("local_favourite"));
    data.rawObject.remove(QStringLiteral("local_favourite"));
  }

  // 2. local_snooze_until
  if (existing.snoozeUntil.isValid()) {
    data.snoozeUntil = existing.snoozeUntil;
    incomingObj[QStringLiteral("local_snooze_until")] = existing.snoozeUntil.toString(Qt::ISODate);
    data.rawObject[QStringLiteral("local_snooze_until")] = existing.snoozeUntil.toString(Qt::ISODate);
  } else {
    data.snoozeUntil = QDateTime();
    incomingObj.remove(QStringLiteral("local_snooze_until"));
    data.rawObject.remove(QStringLiteral("local_snooze_until"));
  }

  // 3. local_refreshInterval
  if (existing.refreshInterval.has_value()) {
    data.refreshInterval = existing.refreshInterval;
    incomingObj[QStringLiteral("local_refreshInterval")] = existing.refreshInterval.value();
    data.rawObject[QStringLiteral("local_refreshInterval")] = existing.refreshInterval.value();
  } else if (existing.rawObject.contains(QStringLiteral("local_refreshInterval"))) {
    int interval = existing.rawObject.value(QStringLiteral("local_refreshInterval")).toInt();
    data.refreshInterval = interval;
    incomingObj[QStringLiteral("local_refreshInterval")] = interval;
    data.rawObject[QStringLiteral("local_refreshInterval")] = interval;
  } else {
    data.refreshInterval = std::nullopt;
    incomingObj.remove(QStringLiteral("local_refreshInterval"));
    data.rawObject.remove(QStringLiteral("local_refreshInterval"));
  }

  // 4. lastRefreshed
  if (isSuccessfulRefresh) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    data.lastRefreshed = now;
    incomingObj[QStringLiteral("lastRefreshed")] = now.toString(Qt::ISODate);
    data.rawObject[QStringLiteral("lastRefreshed")] = now.toString(Qt::ISODate);
  } else {
    if (existing.lastRefreshed.isValid()) {
      data.lastRefreshed = existing.lastRefreshed;
      incomingObj[QStringLiteral("lastRefreshed")] = existing.lastRefreshed.toString(Qt::ISODate);
      data.rawObject[QStringLiteral("lastRefreshed")] = existing.lastRefreshed.toString(Qt::ISODate);
    } else if (incomingObj.contains(QStringLiteral("lastRefreshed"))) {
      data.lastRefreshed =
          QDateTime::fromString(incomingObj.value(QStringLiteral("lastRefreshed")).toString(), Qt::ISODate);
      if (data.lastRefreshed.isValid()) {
        data.rawObject[QStringLiteral("lastRefreshed")] = data.lastRefreshed.toString(Qt::ISODate);
      }
    }
  }
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
      return session.title.simplified();
    case ColState:
      return session.state;
    case ColChangeSet:
      return session.hasChangeSet ? i18n("has changes set") : QString();
    case ColPR:
      return session.prNumber;
    case ColPRStatus:
      return session.prStatus;
    case ColPRLabels:
      return session.prLabels.join(QStringLiteral(", "));
    case ColUpdatedAt:
      return session.updateTime.toString(QLocale::system().dateFormat(QLocale::ShortFormat));
    case ColCreatedAt:
      return session.createTime.toString(QLocale::system().dateFormat(QLocale::ShortFormat));
    case ColOwner:
      return session.owner;
    case ColRepo:
      return session.repo;
    case ColId:
      return session.id;
    case ColLastRefreshed: {
      QDateTime lr = session.lastRefreshed.isValid() ? session.lastRefreshed : session.updateTime;
      if (!lr.isValid())
        return QVariant();
      qint64 secs = lr.secsTo(QDateTime::currentDateTimeUtc());
      QString timeStr;
      if (secs < 60) {
        timeStr = i18np("1 sec ago", "%1 secs ago", secs);
      } else if (secs < 3600) {
        timeStr = i18np("1 min ago", "%1 mins ago", secs / 60);
      } else {
        timeStr = i18np("1 hour ago", "%1 hours ago", secs / 3600);
      }
      return i18n("%1 at %2", timeStr,
                  lr.toLocalTime().toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat)));
    }
    default:
      return QVariant();
    }
  } else if (role == Qt::ForegroundRole) {
    if (index.column() == ColPR && !session.prNumber.isEmpty()) {
      return QColor(Qt::blue);
    }
  } else if (role == Qt::DecorationRole) {
    if (index.column() == ColTitle && session.favouriteRank.has_value()) {
      return QIcon::fromTheme(QStringLiteral("emblem-favorite"));
    }
    return QVariant();
  } else if (role == Qt::ToolTipRole) {
    if (index.column() == ColTitle && session.favouriteRank.has_value()) {
      return i18n("Favourite Rank: %1", session.favouriteRank.value());
    }
    return QVariant();
  } else if (role == Qt::FontRole) {
    bool hasUnread = session.hasUnreadChanges;
    bool hasPR = (index.column() == ColPR && !session.prNumber.isEmpty());
    bool modified = false;

    if (index.column() == ColPR && !session.prNumber.isEmpty()) {
      modified = true;
    }
    if (session.hasUnreadChanges) {
      modified = true;
    }

    if (hasUnread || hasPR || modified) {
      QFont font;
      if (hasUnread)
        font.setBold(true);
      if (hasPR)
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
    return session.title.simplified();
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
  case LastRefreshedRole:
    return session.lastRefreshed.isValid() ? QVariant(session.lastRefreshed) : QVariant();
  case PrStatusRole:
    return session.prStatus;
  case PrLabelsRole:
    return session.prLabels;
  case FavouriteRole:
    return session.favouriteRank.has_value() ? QVariant(session.favouriteRank.value()) : QVariant();
  case SnoozeUntilRole:
    return session.snoozeUntil.isValid() ? QVariant(session.snoozeUntil) : QVariant();
  case UnreadChangesRole:
    return session.hasUnreadChanges;
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
    case ColPRStatus:
      return i18n("PR Status");
    case ColPRLabels:
      return i18n("Labels");
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
  roles[FavouriteRole] = "favourite";
  roles[SnoozeUntilRole] = "snoozeUntil";
  return roles;
}

void SessionModel::toggleFavourite(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    if (data.favouriteRank.has_value() && data.favouriteRank.value() > 0) {
      data.favouriteRank = std::nullopt;
      data.rawObject.remove(QStringLiteral("local_favourite"));
    } else {
      data.favouriteRank = 1;
      data.rawObject[QStringLiteral("local_favourite")] = 1;
    }
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
  }
}

void SessionModel::setFavouriteRank(const QString &id, int rank) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    data.favouriteRank = rank;
    data.rawObject[QStringLiteral("local_favourite")] = rank;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
  }
}

void SessionModel::increaseFavouriteRank(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    int currentRank = data.favouriteRank.value_or(0);
    data.favouriteRank = currentRank + 1;
    data.rawObject[QStringLiteral("local_favourite")] = currentRank + 1;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
  }
}

void SessionModel::decreaseFavouriteRank(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    int currentRank = data.favouriteRank.value_or(0);
    int newRank = currentRank - 1;
    if (newRank <= 0) {
      data.favouriteRank = std::nullopt;
      data.rawObject.remove(QStringLiteral("local_favourite"));
    } else {
      data.favouriteRank = newRank;
      data.rawObject[QStringLiteral("local_favourite")] = newRank;
    }
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
  }
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
      const SessionData &existing = m_sessions[row];
      bool wasUnread = existing.hasUnreadChanges;
      QString oldState = existing.state;
      QString oldPrStatus = existing.prStatus;
      QString oldTitle = existing.title;
      bool oldHasChangeSet = existing.hasChangeSet;
      QDateTime oldUpdateTime = existing.updateTime;

      SessionData data = parseSessionData(obj);
      mergeClientFields(existing, obj, data, /*isSuccessfulRefresh=*/false);
      data.id = id;

      bool isUnread = wasUnread || (oldState != data.state) || (oldPrStatus != data.prStatus) ||
                      (oldTitle != data.title) || (oldHasChangeSet != data.hasChangeSet) ||
                      (oldUpdateTime != data.updateTime && data.updateTime.isValid());
      data.hasUnreadChanges = isUnread;

      m_sessions[row] = data;
      Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
    } else {
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
    m_idToIndex[data.id] = m_sessions.size() - 1;
  }
  endInsertRows();
  return newSessions.size();
}

void SessionModel::addSession(const QJsonObject &session) {
  beginInsertRows(QModelIndex(), 0, 0);
  SessionData data = parseSessionData(session);
  m_sessions.insert(0, data);
  m_idToIndex.clear();
  for (int i = 0; i < m_sessions.size(); ++i) {
    m_idToIndex[m_sessions[i].id] = i;
  }
  endInsertRows();
}

void SessionModel::updateSession(const QJsonObject &session, bool isSuccessfulRefresh) {
  QString id = session.value(QStringLiteral("id")).toString();
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    const SessionData &existing = m_sessions[i];
    bool wasUnread = existing.hasUnreadChanges;
    QString oldState = existing.state;
    QString oldPrStatus = existing.prStatus;
    QString oldTitle = existing.title;
    bool oldHasChangeSet = existing.hasChangeSet;
    QDateTime oldUpdateTime = existing.updateTime;

    QJsonObject sessionCopy = session;
    SessionData data = parseSessionData(sessionCopy);
    mergeClientFields(existing, sessionCopy, data, isSuccessfulRefresh);
    data.id = id;

    bool isSubstantiallyChanged = false;
    if (oldState != data.state || oldPrStatus != data.prStatus || oldTitle != data.title ||
        (oldHasChangeSet != data.hasChangeSet) || (oldUpdateTime != data.updateTime && data.updateTime.isValid())) {
      isSubstantiallyChanged = true;
    }

    data.hasUnreadChanges = wasUnread || isSubstantiallyChanged;

    m_sessions[i] = data;
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    return;
  }

  QJsonObject sessionCopy = session;
  if (isSuccessfulRefresh) {
    sessionCopy[QStringLiteral("lastRefreshed")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  }
  addSession(sessionCopy);
}

void SessionModel::updateSessionAt(int row, const QJsonObject &session, bool isSuccessfulRefresh) {
  if (row < 0 || row >= m_sessions.size()) {
    return;
  }
  const SessionData &existing = m_sessions[row];
  QJsonObject sessionCopy = session;
  SessionData replacement = parseSessionData(sessionCopy);
  mergeClientFields(existing, sessionCopy, replacement, isSuccessfulRefresh);
  replacement.hasUnreadChanges = existing.hasUnreadChanges;
  m_sessions[row] = replacement;
  m_idToIndex.clear();
  for (int index = 0; index < m_sessions.size(); ++index) {
    m_idToIndex[m_sessions[index].id] = index;
  }
  Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
  saveSessions();
}

QJsonObject SessionModel::getSession(int row) const {
  if (row >= 0 && row < m_sessions.size()) {
    return m_sessions[row].rawObject;
  }
  return QJsonObject();
}

QString SessionModel::getSessionName(const QString &id) const {
  const int index = m_idToIndex.value(id, -1);
  if (index != -1) {
    const SessionData &data = m_sessions[index];
    return data.name.isEmpty() ? data.title : data.name;
  }
  return {};
}

QJsonArray SessionModel::getAllSessions() const {
  QJsonArray arr;
  for (const SessionData &data : m_sessions) {
    arr.append(data.rawObject);
  }
  return arr;
}

void SessionModel::setSnoozeUntil(const QString &id, const QDateTime &snoozeUntil) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    data.snoozeUntil = snoozeUntil;
    data.rawObject[QStringLiteral("local_snooze_until")] = snoozeUntil.toString(Qt::ISODate);
    QModelIndex index = this->index(i, 0);
    Q_EMIT dataChanged(index, index, {SnoozeUntilRole});
  }
}

void SessionModel::clearSnooze(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    data.snoozeUntil = QDateTime();
    data.rawObject.remove(QStringLiteral("local_snooze_until"));
    QModelIndex index = this->index(i, 0);
    Q_EMIT dataChanged(index, index, {SnoozeUntilRole});
  }
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
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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

void SessionModel::setNextPageToken(const QString &token) { m_nextPageToken = token; }

QString SessionModel::nextPageToken() const { return m_nextPageToken; }

void SessionModel::clearSessions() {
  beginResetModel();
  m_sessions.clear();
  m_idToIndex.clear();
  endResetModel();
}

void SessionModel::clearUnreadChanges() {
  if (m_sessions.isEmpty()) {
    return;
  }
  for (int i = 0; i < m_sessions.size(); ++i) {
    m_sessions[i].hasUnreadChanges = false;
  }
  Q_EMIT dataChanged(index(0, 0), index(m_sessions.size() - 1, ColCount - 1));
}

void SessionModel::markAsRead(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int row = m_idToIndex.value(id);
    if (m_sessions[row].hasUnreadChanges) {
      m_sessions[row].hasUnreadChanges = false;
      Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
    }
  }
}

bool SessionModel::contains(const QString &id) const { return m_idToIndex.contains(id); }

void SessionModel::clearAllUnreadChanges() {
  bool changed = false;
  for (int i = 0; i < m_sessions.size(); ++i) {
    if (m_sessions[i].hasUnreadChanges) {
      m_sessions[i].hasUnreadChanges = false;
      changed = true;
    }
  }
  if (changed) {
    Q_EMIT dataChanged(index(0, 0), index(m_sessions.size() - 1, ColCount - 1));
  }
}

void SessionModel::clearUnreadChanges(const QString &id) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    if (m_sessions[i].hasUnreadChanges) {
      m_sessions[i].hasUnreadChanges = false;
      Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    }
  }
}

void SessionModel::setRefreshInterval(const QString &id, int minutes) {
  if (m_idToIndex.contains(id)) {
    int i = m_idToIndex.value(id);
    SessionData &data = m_sessions[i];
    if (minutes == -1) {
      data.refreshInterval = std::nullopt;
      data.rawObject.remove(QStringLiteral("local_refreshInterval"));
    } else {
      data.refreshInterval = minutes;
      data.rawObject[QStringLiteral("local_refreshInterval")] = minutes;
    }
    Q_EMIT dataChanged(index(i, 0), index(i, ColCount - 1));
    saveSessions();
  }
}
