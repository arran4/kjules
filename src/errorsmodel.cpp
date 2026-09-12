#include "errorsmodel.h"
#include "migrationorchestrator.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

static QString jobKey(const QJsonObject &obj) {
  QString jobId = obj.value(QStringLiteral("jobId")).toString();
  QString attemptId = obj.value(QStringLiteral("attemptId")).toString();
  if (!jobId.isEmpty() || !attemptId.isEmpty()) {
    return jobId + QLatin1Char(':') + attemptId;
  }
  QString sessionId = obj.value(QStringLiteral("sessionId")).toString();
  if (!sessionId.isEmpty()) {
    return sessionId;
  }
  return QString();
}

ErrorsModel::ErrorsModel(QObject *parent, const QString &filename) : QAbstractListModel(parent), m_filename(filename) {
  loadErrors();
}

int ErrorsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_operationalErrors.size() + m_jobErrors.size();
}

QJsonObject ErrorsModel::getError(int row) const {
  if (row >= 0 && row < m_operationalErrors.size()) {
    return m_operationalErrors.at(row);
  }
  int jobRow = row - m_operationalErrors.size();
  if (jobRow >= 0 && jobRow < m_jobErrors.size()) {
    return m_jobErrors.at(jobRow);
  }
  return QJsonObject();
}

QVariant ErrorsModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= rowCount())
    return QVariant();

  const QJsonObject error = getError(index.row());

  switch (role) {
  case RequestRole:
    return error.value(QStringLiteral("request")).toObject();
  case ResponseRole:
    return error.value(QStringLiteral("response")).toObject();
  case MessageRole:
    return error.value(QStringLiteral("message")).toString();
  case HttpDetailsRole:
    return error.value(QStringLiteral("httpDetails")).toString();
  case TimestampRole:
    if (error.contains(QStringLiteral("timestamp"))) {
      QDateTime dt = QDateTime::fromString(error.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
      if (dt.isValid()) {
        return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
      }
    }
    return QVariant();
  case Qt::DisplayRole:
    return error.value(QStringLiteral("message")).toString(); // Display error message as title
  case SeenRole:
    if (index.row() < m_operationalErrors.size()) {
      return m_operationalSeenState.at(index.row());
    } else {
      QString key = jobKey(error);
      return !key.isEmpty() && m_seenJobKeys.contains(key);
    }
  case UnseenRole: {
    bool seen = false;
    if (index.row() < m_operationalErrors.size()) {
      seen = m_operationalSeenState.at(index.row());
    } else {
      QString key = jobKey(error);
      seen = !key.isEmpty() && m_seenJobKeys.contains(key);
    }
    return !seen;
  }
  case SourceIdRole:
    return error.value(QStringLiteral("sourceId")).toString();
  case SessionIdRole:
    return error.value(QStringLiteral("sessionId")).toString();
  case OperationRole:
    return error.value(QStringLiteral("operation")).toString();
  case ProviderRole:
    return error.value(QStringLiteral("provider")).toString();
  case JobIdRole:
    return error.value(QStringLiteral("jobId")).toString();
  case AttemptIdRole:
    return error.value(QStringLiteral("attemptId")).toString();
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> ErrorsModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[RequestRole] = "request";
  roles[ResponseRole] = "response";
  roles[MessageRole] = "message";
  roles[HttpDetailsRole] = "httpDetails";
  roles[TimestampRole] = "timestamp";
  roles[SeenRole] = "seen";
  roles[UnseenRole] = "unseen";
  roles[SourceIdRole] = "sourceId";
  roles[SessionIdRole] = "sessionId";
  roles[OperationRole] = "operation";
  roles[ProviderRole] = "provider";
  roles[JobIdRole] = "jobId";
  roles[AttemptIdRole] = "attemptId";
  return roles;
}

void ErrorsModel::addErrorObj(const QJsonObject &errorObj) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_operationalErrors.insert(0, errorObj);
  m_operationalSeenState.insert(0, false); // New errors start as unseen
  endInsertRows();

  while (m_operationalErrors.size() > 200) {
    int lastIdx = m_operationalErrors.size() - 1;
    beginRemoveRows(QModelIndex(), lastIdx, lastIdx);
    m_operationalErrors.removeLast();
    m_operationalSeenState.removeLast();
    endRemoveRows();
  }

  updateUnseenCount();
  saveErrors();
}

void ErrorsModel::updateError(int row, const QJsonObject &errorObj) {
  if (row >= 0 && row < m_operationalErrors.size()) {
    m_operationalErrors[row] = errorObj;
    Q_EMIT dataChanged(index(row, 0), index(row, 0));
    saveErrors();
  } else {
    int jobRow = row - m_operationalErrors.size();
    if (jobRow >= 0 && jobRow < m_jobErrors.size()) {
      m_jobErrors[jobRow] = errorObj;
      Q_EMIT dataChanged(index(row, 0), index(row, 0));
    }
  }
}

void ErrorsModel::clear() {
  beginResetModel();
  m_operationalErrors.clear();
  m_operationalSeenState.clear();
  m_jobErrors.clear();
  m_seenJobKeys.clear();
  m_dismissedJobKeys.clear();
  endResetModel();
  updateUnseenCount();
  saveErrors();
}

void ErrorsModel::removeError(int row) {
  if (row >= 0 && row < m_operationalErrors.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_operationalErrors.removeAt(row);
    m_operationalSeenState.removeAt(row);
    endRemoveRows();
    updateUnseenCount();
    saveErrors();
  } else {
    int jobRow = row - m_operationalErrors.size();
    if (jobRow >= 0 && jobRow < m_jobErrors.size()) {
      QString key = jobKey(m_jobErrors[jobRow]);
      if (!key.isEmpty()) {
        m_dismissedJobKeys.insert(key);
      }
      beginRemoveRows(QModelIndex(), row, row);
      m_jobErrors.removeAt(jobRow);
      endRemoveRows();
      updateUnseenCount();
    }
  }
}

QString ErrorsModel::cacheFilePath() const {
  if (QFileInfo(m_filename).isAbsolute()) {
    return m_filename;
  }
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return path + QLatin1Char('/') + m_filename;
}

void ErrorsModel::loadErrors() {
  if (MigrationOrchestrator::isMigrated())
    return;

  QString filePath = cacheFilePath();
  QFile file(filePath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray array = doc.array();
    file.close();

    m_operationalErrors.clear();
    for (const QJsonValue &v : array) {
      m_operationalErrors.append(v.toObject());
    }

    // Trim to 200 on load
    bool trimmed = false;
    while (m_operationalErrors.size() > 200) {
      m_operationalErrors.removeLast();
      trimmed = true;
    }
    if (trimmed) {
      saveErrors();
    }
  }

  m_operationalSeenState.clear();
  for (int i = 0; i < m_operationalErrors.size(); ++i) {
    m_operationalSeenState.append(true); // Loaded errors start as seen
  }
  updateUnseenCount();
}

void ErrorsModel::saveErrors() {
  if (MigrationOrchestrator::isMigrated())
    return;

  QString filePath = cacheFilePath();
  QFileInfo fileInfo(filePath);
  QDir dir = fileInfo.dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonArray array;
    for (const QJsonObject &obj : m_operationalErrors) {
      array.append(obj);
    }
    QJsonDocument doc(array);
    file.write(doc.toJson());
    file.close();
  }
}

int ErrorsModel::unseenCount() const { return m_unseenCount; }

void ErrorsModel::updateUnseenCount() {
  int count = 0;
  for (bool seen : m_operationalSeenState) {
    if (!seen)
      count++;
  }
  for (const QJsonObject &jobErr : m_jobErrors) {
    QString key = jobKey(jobErr);
    if (key.isEmpty() || !m_seenJobKeys.contains(key)) {
      count++;
    }
  }
  if (m_unseenCount != count) {
    m_unseenCount = count;
    Q_EMIT unseenCountChanged(m_unseenCount);
  }
}

void ErrorsModel::markSeen(int row) {
  if (row >= 0 && row < m_operationalErrors.size()) {
    if (!m_operationalSeenState[row]) {
      m_operationalSeenState[row] = true;
      Q_EMIT dataChanged(index(row, 0), index(row, 0), {SeenRole, UnseenRole});
      updateUnseenCount();
    }
  } else {
    int jobRow = row - m_operationalErrors.size();
    if (jobRow >= 0 && jobRow < m_jobErrors.size()) {
      QString key = jobKey(m_jobErrors[jobRow]);
      if (!key.isEmpty() && !m_seenJobKeys.contains(key)) {
        m_seenJobKeys.insert(key);
        Q_EMIT dataChanged(index(row, 0), index(row, 0), {SeenRole, UnseenRole});
        updateUnseenCount();
      }
    }
  }
}

void ErrorsModel::markAllSeen() {
  bool changed = false;
  for (int i = 0; i < m_operationalSeenState.size(); ++i) {
    if (!m_operationalSeenState[i]) {
      m_operationalSeenState[i] = true;
      changed = true;
    }
  }
  for (const QJsonObject &jobErr : m_jobErrors) {
    QString key = jobKey(jobErr);
    if (!key.isEmpty() && !m_seenJobKeys.contains(key)) {
      m_seenJobKeys.insert(key);
      changed = true;
    }
  }
  if (changed) {
    int total = rowCount();
    if (total > 0) {
      Q_EMIT dataChanged(index(0, 0), index(total - 1, 0), {SeenRole, UnseenRole});
    }
    updateUnseenCount();
  }
}

void ErrorsModel::setErrors(const QJsonArray &errors) {
  beginResetModel();
  m_operationalErrors.clear();
  for (const QJsonValue &v : errors) {
    m_operationalErrors.append(v.toObject());
  }
  m_operationalSeenState.clear();
  for (int i = 0; i < m_operationalErrors.size(); ++i)
    m_operationalSeenState.append(true);
  endResetModel();
  updateUnseenCount();
}

void ErrorsModel::syncJobErrors(const QJsonArray &jobErrors) {
  QVector<QJsonObject> newJobErrors;
  newJobErrors.reserve(jobErrors.size());
  for (const QJsonValue &val : jobErrors) {
    QJsonObject obj = val.toObject();
    QString key = jobKey(obj);
    if (!key.isEmpty() && m_dismissedJobKeys.contains(key)) {
      continue;
    }
    newJobErrors.append(obj);
  }

  if (newJobErrors == m_jobErrors) {
    return;
  }

  beginResetModel();
  m_jobErrors = newJobErrors;
  endResetModel();
  updateUnseenCount();
}
