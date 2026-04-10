#include "errorsmodel.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

ErrorsModel::ErrorsModel(QObject *parent) : QAbstractListModel(parent) {
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
      QDateTime dt = QDateTime::fromString(
          error.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
      if (dt.isValid()) {
        return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
      }
    }
    return QVariant();
  case Qt::DisplayRole:
    return error.value(QStringLiteral("message"))
        .toString(); // Display error message as title
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
  return roles;
}

void ErrorsModel::addError(const QJsonObject &request,
                           const QJsonObject &response, const QString &message,
                           const QString &httpDetails) {
  QJsonObject errorObj;
  errorObj[QStringLiteral("request")] = request;
  errorObj[QStringLiteral("response")] = response;
  errorObj[QStringLiteral("message")] = message;
  if (!httpDetails.isEmpty()) {
    errorObj[QStringLiteral("httpDetails")] = httpDetails;
  }
  errorObj[QStringLiteral("timestamp")] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  addErrorObj(errorObj);
}

void ErrorsModel::addErrorObj(const QJsonObject &errorObj) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_errors.insert(0, errorObj);
  endInsertRows();
  saveErrors();
}

void ErrorsModel::clear() {
  beginResetModel();
  m_errors = QJsonArray();
  endResetModel();
}

void ErrorsModel::removeError(int row) {
  if (row >= 0 && row < m_errors.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_errors.removeAt(row);
    endRemoveRows();
    saveErrors();
  }
}

QJsonObject ErrorsModel::getError(int row) const {
  if (row >= 0 && row < m_errors.size()) {
    return m_errors[row].toObject();
  }
  return QJsonObject();
}

void ErrorsModel::loadErrors() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/errors.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_errors = doc.array();
    file.close();
  }
}

void ErrorsModel::saveErrors() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(path + QStringLiteral("/errors.json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonDocument doc(m_errors);
    file.write(doc.toJson());
    file.close();
  }
}
