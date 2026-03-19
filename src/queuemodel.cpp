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
  obj[QStringLiteral("lastResponse")] = lastResponse;
  if (lastTry.isValid()) {
    obj[QStringLiteral("lastTry")] = lastTry.toString(Qt::ISODate);
  }
  obj[QStringLiteral("isWaitItem")] = isWaitItem;
  obj[QStringLiteral("isDailyLimitWait")] = isDailyLimitWait;
  obj[QStringLiteral("waitSeconds")] = waitSeconds;
  if (waitStartTime.isValid()) {
    obj[QStringLiteral("waitStartTime")] = waitStartTime.toString(Qt::ISODate);
  }
  return obj;
}

QueueItem QueueItem::fromJson(const QJsonObject &obj) {
  QueueItem item;
  item.requestData = obj.value(QStringLiteral("requestData")).toObject();
  item.errorCount = obj.value(QStringLiteral("errorCount")).toInt();
  item.lastError = obj.value(QStringLiteral("lastError")).toString();
  item.lastResponse = obj.value(QStringLiteral("lastResponse")).toString();
  if (obj.contains(QStringLiteral("lastTry"))) {
    item.lastTry = QDateTime::fromString(
        obj.value(QStringLiteral("lastTry")).toString(), Qt::ISODate);
  }
  item.isWaitItem = obj.value(QStringLiteral("isWaitItem")).toBool();
  item.isDailyLimitWait = obj.value(QStringLiteral("isDailyLimitWait")).toBool();
  item.waitSeconds = obj.value(QStringLiteral("waitSeconds")).toInt();
  if (obj.contains(QStringLiteral("waitStartTime"))) {
    item.waitStartTime = QDateTime::fromString(
        obj.value(QStringLiteral("waitStartTime")).toString(), Qt::ISODate);
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
    if (item.isWaitItem) {
        return i18n("Wait for %1 seconds", item.waitSeconds);
    }
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
    if (item.isWaitItem) {
        if (item.waitStartTime.isValid()) {
            qint64 elapsed = item.waitStartTime.secsTo(QDateTime::currentDateTimeUtc());
            qint64 remaining = item.waitSeconds - elapsed;
            if (remaining > 0) {
                return i18n("Waiting... %1s remaining", remaining);
            } else {
                return i18n("Wait complete");
            }
        }
        return i18n("Pending wait");
    }
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

#include <KSharedConfig>
#include <KConfigGroup>

void QueueModel::enqueue(const QJsonObject &requestData) {
  beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
  QueueItem item;
  item.requestData = requestData;
  m_items.append(item);
  endInsertRows();
  m_jobsSinceLastWait++;

  KConfigGroup config(KSharedConfig::openConfig(), "General");
  QString tier = config.readEntry("Tier", QStringLiteral("free"));
  int jobsBeforeWait = 3;
  if (tier == QStringLiteral("pro")) {
      jobsBeforeWait = 15;
  } else if (tier == QStringLiteral("max")) {
      jobsBeforeWait = 30;
  }

  pruneRunTimestamps();

  int waitTime = config.readEntry("WaitTime", 3600);

  if (m_jobsSinceLastWait >= jobsBeforeWait) {
      beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
      QueueItem waitItem;
      waitItem.isWaitItem = true;
      waitItem.waitSeconds = waitTime;
      m_items.append(waitItem);
      endInsertRows();
      m_jobsSinceLastWait = 0;
  }

  save();
}

void QueueModel::updateItem(int index, const QueueItem &item) {
  if (index >= 0 && index < m_items.size()) {
      m_items[index] = item;
      QModelIndex idx = this->index(index, 0);
      Q_EMIT dataChanged(idx, idx);
      save();
  }
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

void QueueModel::requeueFailed(const QueueItem &item, const QString &errorMsg,
                               const QString &rawResponse) {
  // Usually we want failed items to stay at the front of the queue to be
  // retried
  QueueItem updatedItem = item;
  updatedItem.errorCount++;
  updatedItem.lastError = errorMsg;
  updatedItem.lastResponse = rawResponse;
  updatedItem.lastTry = QDateTime::currentDateTimeUtc();

  beginInsertRows(QModelIndex(), 0, 0);
  m_items.prepend(updatedItem);
  endInsertRows();
  save();
}

void QueueModel::requeueTransient(const QueueItem &item) {
  // Place the item back at the front without recording an error
  beginInsertRows(QModelIndex(), 0, 0);
  m_items.prepend(item);
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


void QueueModel::prependWaitItem(const QueueItem &item) {
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();
    save();
}

void QueueModel::pruneRunTimestamps() {
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-24 * 3600);
    while (!m_runTimestamps.isEmpty() && m_runTimestamps.first() < cutoff) {
        m_runTimestamps.removeFirst();
    }
}

void QueueModel::recordRun() {
    m_runTimestamps.append(QDateTime::currentDateTimeUtc());
    pruneRunTimestamps();
    save();
}

void QueueModel::checkAndPrependDailyLimitWait() {
    pruneRunTimestamps();

    KConfigGroup config(KSharedConfig::openConfig(), "General");
    QString tier = config.readEntry("Tier", QStringLiteral("free"));
    int dailyLimit = 15;
    if (tier == QStringLiteral("pro")) {
        dailyLimit = 100;
    } else if (tier == QStringLiteral("max")) {
        dailyLimit = 300;
    }

    if (m_runTimestamps.size() >= dailyLimit) {
        bool hasDailyLimitWait = false;
        for (const QueueItem& existingItem : qAsConst(m_items)) {
            if (existingItem.isWaitItem && existingItem.isDailyLimitWait) {
                hasDailyLimitWait = true;
                break;
            }
        }

        if (!hasDailyLimitWait) {
            QueueItem waitItem;
            waitItem.isWaitItem = true;
            waitItem.isDailyLimitWait = true;

            qint64 secondsUntilNext = 12 * 3600; // Default fallback
            if (!m_runTimestamps.isEmpty()) {
                QDateTime oldest = m_runTimestamps.first();
                QDateTime nextAvailable = oldest.addSecs(24 * 3600);
                qint64 diff = QDateTime::currentDateTimeUtc().secsTo(nextAvailable);
                if (diff > 0) {
                    secondsUntilNext = diff;
                }
            }

            waitItem.waitSeconds = secondsUntilNext;
            prependWaitItem(waitItem);
        }
    }
}

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
  QJsonArray arr;

  if (doc.isObject()) {
      QJsonObject topObj = doc.object();
      if (topObj.contains(QStringLiteral("m_jobsSinceLastWait"))) {
          m_jobsSinceLastWait = topObj.value(QStringLiteral("m_jobsSinceLastWait")).toInt();
      }
      if (topObj.contains(QStringLiteral("m_runTimestamps"))) {
          QJsonArray tsArr = topObj.value(QStringLiteral("m_runTimestamps")).toArray();
          m_runTimestamps.clear();
          for (int i = 0; i < tsArr.size(); ++i) {
              m_runTimestamps.append(QDateTime::fromString(tsArr[i].toString(), Qt::ISODate));
          }
      }
      arr = topObj.value(QStringLiteral("items")).toArray();
  } else if (doc.isArray()) {
      arr = doc.array();
  }

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

  QJsonObject topObj;
  topObj[QStringLiteral("m_jobsSinceLastWait")] = m_jobsSinceLastWait;

  QJsonArray tsArr;
  for (const QDateTime &dt : qAsConst(m_runTimestamps)) {
      tsArr.append(dt.toString(Qt::ISODate));
  }
  topObj[QStringLiteral("m_runTimestamps")] = tsArr;
  topObj[QStringLiteral("items")] = arr;

  QJsonDocument doc(topObj);
  file.write(doc.toJson());
}
