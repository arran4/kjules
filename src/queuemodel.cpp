#include "queuemodel.h"

#include <KLocalizedString>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

QJsonObject QueueItem::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("requestData")] = requestData;
  obj[QStringLiteral("errorCount")] = errorCount;
  obj[QStringLiteral("lastError")] = lastError;
  if (lastTry.isValid()) {
    obj[QStringLiteral("lastTry")] = lastTry.toString(Qt::ISODate);
  }
  return obj;
}

QueueItem QueueItem::fromJson(const QJsonObject &obj) {
  QueueItem item;
  item.requestData = obj.value(QStringLiteral("requestData")).toObject();
  item.errorCount = obj.value(QStringLiteral("errorCount")).toInt();
  item.lastError = obj.value(QStringLiteral("lastError")).toString();
  if (obj.contains(QStringLiteral("lastTry"))) {
    item.lastTry = QDateTime::fromString(
        obj.value(QStringLiteral("lastTry")).toString(), Qt::ISODate);
  }
  return item;
}

QueueModel::QueueModel(QObject *parent) : QAbstractListModel(parent) { load(); }

int QueueModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_items.size();
}

QVariant QueueModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_items.size() || index.row() < 0) {
    return QVariant();
  }

  const QueueItem &item = m_items.at(index.row());

  switch (role) {
  case RequestDataRole:
    return item.requestData;
  case ErrorCountRole:
    return item.errorCount;
  case LastErrorRole:
    return item.lastError;
  case LastTryRole:
    return item.lastTry;
  case SummaryRole: {
    QString source =
        item.requestData.value(QStringLiteral("source")).toString();
    QString prompt =
        item.requestData.value(QStringLiteral("prompt")).toString();
    // truncate prompt for summary
    if (prompt.length() > 50) {
      prompt = prompt.left(50) + QStringLiteral("...");
    }
    return QStringLiteral("%1: %2").arg(source, prompt);
  }
  case StatusRole: {
    if (item.errorCount > 0) {
      QString timeStr = item.lastTry.isValid()
                            ? item.lastTry.toString(Qt::DefaultLocaleShortDate)
                            : i18n("Unknown time");
      return i18n("Failed %1 times (Last: %2). Error: %3", item.errorCount,
                  timeStr, item.lastError);
    } else {
      return i18n("Pending");
    }
  }
  case Qt::DisplayRole:
    return data(index, SummaryRole);
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> QueueModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[RequestDataRole] = "requestData";
  roles[ErrorCountRole] = "errorCount";
  roles[LastErrorRole] = "lastError";
  roles[LastTryRole] = "lastTry";
  roles[SummaryRole] = "summary";
  roles[StatusRole] = "status";
  return roles;
}

void QueueModel::enqueue(const QJsonObject &requestData) {
  beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
  QueueItem item;
  item.requestData = requestData;
  m_items.append(item);
  endInsertRows();
  save();
}

QueueItem QueueModel::dequeue() {
  if (m_items.isEmpty()) {
    return QueueItem();
  }
  beginRemoveRows(QModelIndex(), 0, 0);
  QueueItem item = m_items.takeFirst();
  endRemoveRows();
  save();
  return item;
}

QueueItem QueueModel::peek() const {
  if (m_items.isEmpty()) {
    return QueueItem();
  }
  return m_items.first();
}

void QueueModel::requeueFailed(const QueueItem &item, const QString &errorMsg) {
  // Usually we want failed items to stay at the front of the queue to be
  // retried
  QueueItem updatedItem = item;
  updatedItem.errorCount++;
  updatedItem.lastError = errorMsg;
  updatedItem.lastTry = QDateTime::currentDateTimeUtc();

  beginInsertRows(QModelIndex(), 0, 0);
  m_items.prepend(updatedItem);
  endInsertRows();
  save();
}

void QueueModel::removeItem(int index) {
  if (index >= 0 && index < m_items.size()) {
    beginRemoveRows(QModelIndex(), index, index);
    m_items.removeAt(index);
    endRemoveRows();
    save();
  }
}

QueueItem QueueModel::getItem(int index) const {
  if (index >= 0 && index < m_items.size()) {
    return m_items.at(index);
  }
  return QueueItem();
}

bool QueueModel::isEmpty() const { return m_items.isEmpty(); }

int QueueModel::size() const { return m_items.size(); }

void QueueModel::clear() {
  if (!m_items.isEmpty()) {
    beginResetModel();
    m_items.clear();
    endResetModel();
    save();
  }
}

void QueueModel::load() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/queue.json");
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  QByteArray data = file.readAll();
  QJsonDocument doc(QJsonDocument::fromJson(data));
  QJsonArray arr = doc.array();

  beginResetModel();
  m_items.clear();
  for (int i = 0; i < arr.size(); ++i) {
    m_items.append(QueueItem::fromJson(arr[i].toObject()));
  }
  endResetModel();
}

void QueueModel::save() {
  QString dirPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(dirPath);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QString path = dirPath + QStringLiteral("/queue.json");
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open queue.json for writing:"
               << file.errorString();
    return;
  }

  file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

  QJsonArray arr;
  for (const QueueItem &item : qAsConst(m_items)) {
    arr.append(item.toJson());
  }

  QJsonDocument doc(arr);
  file.write(doc.toJson());
}
