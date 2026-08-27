#include "errorsmodel.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

ErrorsModel::ErrorsModel(QObject *parent, const QString &filename) : QAbstractListModel(parent), m_filename(filename) {
  loadErrors();
}

int ErrorsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_errors.size();
}

QVariant ErrorsModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_errors.size())
    return QVariant();

  const QJsonObject error = m_errors[index.row()].toObject();

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
    return m_seenState.at(index.row());
  case UnseenRole:
    return !m_seenState.at(index.row());
  case SourceIdRole:
    return error.value(QStringLiteral("sourceId")).toString();
  case SessionIdRole:
    return error.value(QStringLiteral("sessionId")).toString();
  case OperationRole:
    return error.value(QStringLiteral("operation")).toString();
  case ProviderRole:
    return error.value(QStringLiteral("provider")).toString();
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
  return roles;
}

void ErrorsModel::addErrorObj(const QJsonObject &errorObj) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_errors.insert(0, errorObj);
  m_seenState.insert(0, false); // New errors start as unseen
  endInsertRows();

  while (m_errors.size() > 200) {
    beginRemoveRows(QModelIndex(), m_errors.size() - 1, m_errors.size() - 1);
    m_errors.removeLast();
    m_seenState.removeLast();
    endRemoveRows();
  }

  updateUnseenCount();
  saveErrors();
}

void ErrorsModel::updateError(int row, const QJsonObject &errorObj) {
  if (row < 0 || row >= m_errors.size()) {
    return;
  }
  m_errors[row] = errorObj;
  Q_EMIT dataChanged(index(row, 0), index(row, 0));
  saveErrors();
}

void ErrorsModel::clear() {
  beginResetModel();
  m_errors = QJsonArray();
  m_seenState.clear();
  endResetModel();
  updateUnseenCount();
  saveErrors();
}

void ErrorsModel::removeError(int row) {
  if (row >= 0 && row < m_errors.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_errors.removeAt(row);
    m_seenState.removeAt(row);
    endRemoveRows();
    updateUnseenCount();
    saveErrors();
  }
}

QJsonObject ErrorsModel::getError(int row) const {
  if (row >= 0 && row < m_errors.size()) {
    return m_errors[row].toObject();
  }
  return QJsonObject();
}

QString ErrorsModel::cacheFilePath() const {
  if (QFileInfo(m_filename).isAbsolute()) {
    return m_filename;
  }
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return path + QLatin1Char('/') + m_filename;
}

void ErrorsModel::loadErrors() {
  QString filePath = cacheFilePath();
  QFile file(filePath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_errors = doc.array();
    file.close();

    // Trim to 200 on load
    bool trimmed = false;
    while (m_errors.size() > 200) {
      m_errors.removeLast();
      trimmed = true;
    }
    if (trimmed) {
      saveErrors();
    }
  }

  m_seenState.clear();
  for (int i = 0; i < m_errors.size(); ++i) {
    m_seenState.append(true); // Loaded errors start as seen
  }
  updateUnseenCount();
}

void ErrorsModel::saveErrors() {
  QString filePath = cacheFilePath();
  QFileInfo fileInfo(filePath);
  QDir dir = fileInfo.dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonDocument doc(m_errors);
    file.write(doc.toJson());
    file.close();
  }
}

int ErrorsModel::unseenCount() const { return m_unseenCount; }

void ErrorsModel::updateUnseenCount() {
  int count = 0;
  for (bool seen : m_seenState) {
    if (!seen)
      count++;
  }
  if (m_unseenCount != count) {
    m_unseenCount = count;
    Q_EMIT unseenCountChanged(m_unseenCount);
  }
}

void ErrorsModel::markSeen(int row) {
  if (row >= 0 && row < m_seenState.size() && !m_seenState[row]) {
    m_seenState[row] = true;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {SeenRole, UnseenRole});
    updateUnseenCount();
  }
}

void ErrorsModel::markAllSeen() {
  bool changed = false;
  for (int i = 0; i < m_seenState.size(); ++i) {
    if (!m_seenState[i]) {
      m_seenState[i] = true;
      changed = true;
    }
  }
  if (changed) {
    Q_EMIT dataChanged(index(0, 0), index(m_seenState.size() - 1, 0), {SeenRole, UnseenRole});
    updateUnseenCount();
  }
}
