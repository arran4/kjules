#include "queuemodel.h"
#include <utility>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <limits>

qint64 QueueModel::maxBackoffSeconds() {
  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));
  qint64 maxWait =
      queueConfig.readEntry("BackoffMax", 480) * 60; // Default 8 hours
  if (maxWait < 0)
    maxWait = 8 * 60 * 60; // Failsafe
  return maxWait;
}

static qint64 calculateExponentialBackoff(qint64 initialWait, int expBase,
                                          int errorCount) {
  int power = qMax(0, errorCount - 1);
  if (power > 20)
    power = 20; // Prevent huge shifts
  qint64 multiplier = 1;
  bool overflow = false;
  for (int i = 0; i < power; ++i) {
    if (multiplier > std::numeric_limits<qint64>::max() / expBase) {
      overflow = true;
      break;
    }
    multiplier *= expBase;
  }

  if (overflow ||
      initialWait > std::numeric_limits<qint64>::max() / multiplier) {
    return std::numeric_limits<qint64>::max();
  }
  return initialWait * multiplier;
}

static qint64 calculateRandomBackoff(KConfigGroup &queueConfig) {
  int randMin = queueConfig.readEntry("BackoffRandomMin", 10) * 60;
  int randMax = queueConfig.readEntry("BackoffRandomMax", 60) * 60;
  if (randMax > randMin) {
    return QRandomGenerator::global()->bounded(randMin, randMax + 1);
  }
  return randMin;
}

static qint64 calculateFixedBackoff(qint64 initialWait) { return initialWait; }

qint64 QueueModel::calculateBackoff(int errorCount) {
  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));

  QString type = queueConfig.readEntry("BackoffType", QStringLiteral("fixed"));
  qint64 initialWait =
      queueConfig.readEntry("BackoffInterval", 30) * 60; // Default 30 mins
  qint64 waitSeconds = initialWait;

  if (type == QStringLiteral("exponential")) {
    int expBase = queueConfig.readEntry("BackoffExpBase", 2);
    waitSeconds = calculateExponentialBackoff(initialWait, expBase, errorCount);
  } else if (type == QStringLiteral("random")) {
    waitSeconds = calculateRandomBackoff(queueConfig);
  } else {
    waitSeconds = calculateFixedBackoff(initialWait);
  }

  return qMin(waitSeconds, maxBackoffSeconds());
}

QJsonObject QueueItem::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("requestData")] = requestData;
  obj[QStringLiteral("errorCount")] = errorCount;
  obj[QStringLiteral("lastError")] = lastError;
  obj[QStringLiteral("lastResponse")] = lastResponse;
  if (lastTry.isValid()) {
    obj[QStringLiteral("lastTry")] = lastTry.toString(Qt::ISODate);
  }
  if (!pastErrors.isEmpty()) {
    obj[QStringLiteral("pastErrors")] = pastErrors;
  }
  obj[QStringLiteral("isWaitItem")] = isWaitItem;
  obj[QStringLiteral("isDailyLimitWait")] = isDailyLimitWait;
  obj[QStringLiteral("waitSeconds")] = waitSeconds;
  if (waitStartTime.isValid()) {
    obj[QStringLiteral("waitStartTime")] = waitStartTime.toString(Qt::ISODate);
  }
  obj[QStringLiteral("isBlocked")] = isBlocked;
  if (!blockMetadata.isEmpty()) {
    obj[QStringLiteral("blockMetadata")] = blockMetadata;
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
  if (obj.contains(QStringLiteral("pastErrors"))) {
    item.pastErrors = obj.value(QStringLiteral("pastErrors")).toArray();
  }
  item.isWaitItem = obj.value(QStringLiteral("isWaitItem")).toBool();
  item.isDailyLimitWait =
      obj.value(QStringLiteral("isDailyLimitWait")).toBool();
  item.waitSeconds = obj.value(QStringLiteral("waitSeconds")).toInt();
  if (obj.contains(QStringLiteral("waitStartTime"))) {
    item.waitStartTime = QDateTime::fromString(
        obj.value(QStringLiteral("waitStartTime")).toString(), Qt::ISODate);
  }
  item.isBlocked = obj.value(QStringLiteral("isBlocked")).toBool();
  if (obj.contains(QStringLiteral("blockMetadata"))) {
    item.blockMetadata = obj.value(QStringLiteral("blockMetadata")).toObject();
  }
  return item;
}

#include <QDataStream>
#include <QMimeData>

QueueModel::QueueModel(QObject *parent, const QString &filename, bool isHolding)
    : QAbstractListModel(parent), m_filename(filename), m_isHolding(isHolding) {
  load();
}

int QueueModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_items.size();
}

Qt::ItemFlags QueueModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
  if (index.isValid()) {
    return defaultFlags | Qt::ItemIsDragEnabled;
  }
  return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions QueueModel::supportedDropActions() const {
  return Qt::MoveAction;
}

QStringList QueueModel::mimeTypes() const {
  return {QStringLiteral("application/x-kjules-queue-item")};
}

QMimeData *QueueModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mimeData = new QMimeData();
  QByteArray encodedData;
  QDataStream stream(&encodedData, QIODevice::WriteOnly);

  QJsonArray itemsArray;
  QList<int> sourceRows;
  for (const QModelIndex &index : indexes) {
    if (index.isValid()) {
      itemsArray.append(m_items.at(index.row()).toJson());
      sourceRows.append(index.row());
    }
  }

  // Encode both the item data and original rows (useful for internal move
  // detection)
  stream << QJsonDocument(itemsArray).toJson(QJsonDocument::Compact);
  // Also encode source model pointer to detect internal moves
  stream << reinterpret_cast<quintptr>(this);
  for (int r : std::as_const(sourceRows))
    stream << r;

  mimeData->setData(QStringLiteral("application/x-kjules-queue-item"),
                    encodedData);
  return mimeData;
}

bool QueueModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column, const QModelIndex &parent) {
  Q_UNUSED(column);
  if (action == Qt::IgnoreAction)
    return true;

  if (!data->hasFormat(QStringLiteral("application/x-kjules-queue-item")))
    return false;

  int destRow;
  if (row != -1)
    destRow = row;
  else if (parent.isValid())
    destRow = parent.row();
  else
    destRow = rowCount(QModelIndex());

  QByteArray encodedData =
      data->data(QStringLiteral("application/x-kjules-queue-item"));
  QDataStream stream(&encodedData, QIODevice::ReadOnly);

  QByteArray jsonData;
  stream >> jsonData;

  quintptr sourceModelPtr = 0;
  if (!stream.atEnd()) {
    stream >> sourceModelPtr;
  }

  QJsonDocument doc = QJsonDocument::fromJson(jsonData);
  if (!doc.isArray())
    return false;

  QJsonArray itemsArray = doc.array();

  // Internal move handling
  if (sourceModelPtr == reinterpret_cast<quintptr>(this)) {
    QList<int> sourceRows;
    while (!stream.atEnd()) {
      int r;
      stream >> r;
      sourceRows.append(r);
    }

    // Process internal moves manually to avoid Qt ItemViews messing up the data
    // structure. If we return true with MoveAction, Qt calls removeRows. So we
    // return false and do the move manually.
    std::sort(sourceRows.begin(), sourceRows.end());

    // Edge case: moving to same place
    if (sourceRows.size() == 1 &&
        (destRow == sourceRows.first() || destRow == sourceRows.first() + 1)) {
      return false;
    }

    QVector<QueueItem> newItems;
    QVector<QueueItem> movingItems;
    for (int i = 0; i < sourceRows.size(); ++i) {
      movingItems.append(m_items.at(sourceRows[i]));
    }

    int adjustedDestRow = destRow;
    for (int r : std::as_const(sourceRows)) {
      if (r < destRow) {
        adjustedDestRow--;
      }
    }

    beginResetModel();
    for (int i = 0; i < m_items.size(); ++i) {
      if (!sourceRows.contains(i)) {
        newItems.append(m_items[i]);
      }
    }
    for (int i = 0; i < movingItems.size(); ++i) {
      newItems.insert(adjustedDestRow + i, movingItems[i]);
    }
    m_items = newItems;
    endResetModel();
    save();
    return false; // Return false so Qt doesn't delete the original rows
  }

  // External move handling
  int currentDestRow = destRow;
  if (currentDestRow < 0) {
    currentDestRow = m_items.size();
  }
  for (int i = 0; i < itemsArray.size(); ++i) {
    QueueItem newItem = QueueItem::fromJson(itemsArray[i].toObject());
    newItem.isWaitItem = false;
    insertItem(currentDestRow, newItem);
    currentDestRow++;
  }

  return true;
}

bool QueueModel::removeRows(int row, int count, const QModelIndex &parent) {
  if (row < 0 || row + count > m_items.size() || parent.isValid()) {
    return false;
  }
  beginRemoveRows(parent, row, row + count - 1);
  m_items.remove(row, count);
  endRemoveRows();
  save();
  return true;
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
    if (item.isBlocked) {
      return i18n("Blocked");
    }
    if (item.isWaitItem) {
      if (item.waitStartTime.isValid()) {
        qint64 elapsed =
            item.waitStartTime.secsTo(QDateTime::currentDateTimeUtc());
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
      QString timeStr =
          item.lastTry.isValid()
              ? item.lastTry.toString(
                    QLocale::system().dateFormat(QLocale::ShortFormat))
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

#include <KConfigGroup>
#include <KSharedConfig>

void QueueModel::enqueue(const QJsonObject &requestData) {
  QueueItem item;
  item.requestData = requestData;
  enqueueItem(item);
}

void QueueModel::enqueueItem(const QueueItem &item) {
  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  QString tier = config.readEntry("Tier", QStringLiteral("free"));
  int jobsBeforeWait = 3;
  if (tier == QStringLiteral("pro")) {
    jobsBeforeWait = 15;
  } else if (tier == QStringLiteral("max")) {
    jobsBeforeWait = 30;
  }

  pruneRunTimestamps();

  qint64 waitTime = config.readEntry("WaitTime", 3600);

  if (!m_isHolding) {
    if (m_jobsSinceLastWait >= jobsBeforeWait) {
      beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
      QueueItem waitItem;
      waitItem.isWaitItem = true;
      waitItem.waitSeconds = qMin(waitTime, maxBackoffSeconds());
      m_items.append(waitItem);
      endInsertRows();
      m_jobsSinceLastWait = 0;
    }
    m_jobsSinceLastWait++;
  }

  beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
  m_items.append(item);
  endInsertRows();

  save();
}

void QueueModel::insertItem(int index, const QueueItem &item) {
  if (index < 0) {
    index = 0;
  }
  if (index > m_items.size()) {
    index = m_items.size();
  }

  beginInsertRows(QModelIndex(), index, index);
  m_items.insert(index, item);
  endInsertRows();

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
  removeTrailingWaitItems();
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
    removeTrailingWaitItems();
    save();
  }
}

QueueItem QueueModel::getItem(int index) const {
  if (index >= 0 && index < m_items.size()) {
    return m_items.at(index);
  }
  return QueueItem();
}

void QueueModel::moveItem(int from, int to) {
  if (from < 0 || from >= m_items.size() || to < 0 || to >= m_items.size() ||
      from == to) {
    return;
  }

  int destinationChild = (to > from) ? to + 1 : to;

  if (beginMoveRows(QModelIndex(), from, from, QModelIndex(),
                    destinationChild)) {
    if (from < to)
      std::rotate(m_items.begin() + from, m_items.begin() + from + 1,
                  m_items.begin() + to + 1);
    else
      std::rotate(m_items.begin() + to, m_items.begin() + from,
                  m_items.begin() + from + 1);
    endMoveRows();
    save();
  }
}

void QueueModel::refreshWaitItems() {
  int start = -1;
  for (int i = 0; i < m_items.size(); ++i) {
    const auto &item = m_items.at(i);
    if (item.isWaitItem && item.waitStartTime.isValid()) {
      if (start == -1)
        start = i;
    } else if (start != -1) {
      Q_EMIT dataChanged(index(start, 0), index(i - 1, 0), {StatusRole});
      start = -1;
    }
  }
  if (start != -1) {
    Q_EMIT dataChanged(index(start, 0), index(m_items.size() - 1, 0),
                       {StatusRole});
  }
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
  if (m_isHolding) {
    return;
  }
  if (m_items.isEmpty()) {
    return;
  }

  pruneRunTimestamps();

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  QString tier = config.readEntry("Tier", QStringLiteral("free"));
  int dailyLimit = 15;
  if (tier == QStringLiteral("pro")) {
    dailyLimit = 100;
  } else if (tier == QStringLiteral("max")) {
    dailyLimit = 300;
  }

  if (m_runTimestamps.size() >= dailyLimit) {
    bool hasDailyLimitWait = false;
    for (const QueueItem &existingItem : std::as_const(m_items)) {
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

      waitItem.waitSeconds = qMin(secondsUntilNext, maxBackoffSeconds());
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
      QStringLiteral("/") + m_filename;
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
      m_jobsSinceLastWait =
          topObj.value(QStringLiteral("m_jobsSinceLastWait")).toInt();
    }
    if (topObj.contains(QStringLiteral("m_runTimestamps"))) {
      QJsonArray tsArr =
          topObj.value(QStringLiteral("m_runTimestamps")).toArray();
      m_runTimestamps.clear();
      for (int i = 0; i < tsArr.size(); ++i) {
        m_runTimestamps.append(
            QDateTime::fromString(tsArr[i].toString(), Qt::ISODate));
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

  // Clean up trailing wait items from loaded state
  while (!m_items.isEmpty() && m_items.last().isWaitItem) {
    m_items.removeLast();
  }

  endResetModel();
}

void QueueModel::removeTrailingWaitItems() {
  int firstWaitToRemove = m_items.size();
  while (firstWaitToRemove > 0 &&
         m_items.at(firstWaitToRemove - 1).isWaitItem) {
    firstWaitToRemove--;
  }

  if (firstWaitToRemove < m_items.size()) {
    beginRemoveRows(QModelIndex(), firstWaitToRemove, m_items.size() - 1);
    m_items.remove(firstWaitToRemove, m_items.size() - firstWaitToRemove);
    endRemoveRows();
  }
}

void QueueModel::save() {
  QString dirPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(dirPath);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QString path = dirPath + QStringLiteral("/") + m_filename;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open queue.json for writing:"
               << file.errorString();
    return;
  }

  file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

  QJsonArray arr;
  for (const QueueItem &item : std::as_const(m_items)) {
    arr.append(item.toJson());
  }

  QJsonObject topObj;
  topObj[QStringLiteral("m_jobsSinceLastWait")] = m_jobsSinceLastWait;

  QJsonArray tsArr;
  for (const QDateTime &dt : std::as_const(m_runTimestamps)) {
    tsArr.append(dt.toString(Qt::ISODate));
  }
  topObj[QStringLiteral("m_runTimestamps")] = tsArr;
  topObj[QStringLiteral("items")] = arr;

  QJsonDocument doc(topObj);
  file.write(doc.toJson());
}
