#include "queuemodel.h"
#include "utils.h"
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
    item.lastTry = QDateTime::fromString(obj.value(QStringLiteral("lastTry")).toString(), Qt::ISODate);
  }
  if (obj.contains(QStringLiteral("pastErrors"))) {
    item.pastErrors = obj.value(QStringLiteral("pastErrors")).toArray();
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

Qt::DropActions QueueModel::supportedDropActions() const { return Qt::MoveAction; }

QStringList QueueModel::mimeTypes() const { return {QStringLiteral("application/x-kjules-queue-item")}; }

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

  mimeData->setData(QStringLiteral("application/x-kjules-queue-item"), encodedData);
  return mimeData;
}

bool QueueModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*column*/,
                              const QModelIndex &parent) {
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

  QByteArray encodedData = data->data(QStringLiteral("application/x-kjules-queue-item"));
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
    if (sourceRows.size() == 1 && (destRow == sourceRows.first() || destRow == sourceRows.first() + 1)) {
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
    newItems.reserve(m_items.size());
    auto sourceIt = sourceRows.cbegin();
    auto sourceEnd = sourceRows.cend();
    for (int i = 0; i < m_items.size(); ++i) {
      while (sourceIt != sourceEnd && *sourceIt < i) {
        ++sourceIt;
      }
      if (sourceIt == sourceEnd || *sourceIt != i) {
        newItems.append(m_items.at(i));
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
    QString source = item.requestData.value(QStringLiteral("source")).toString();
    QString prompt = item.requestData.value(QStringLiteral("prompt")).toString();
    // truncate prompt for summary
    if (prompt.length() > 50) {
      prompt = prompt.left(50) + QStringLiteral("...");
    }
    int priority = item.requestData.value(QStringLiteral("priority")).toInt(0);
    if (priority != 0) {
      return QStringLiteral("[Priority: %1] %2: %3").arg(priority).arg(source, prompt);
    }
    return QStringLiteral("%1: %2").arg(source, prompt);
  }
  case StatusRole: {
    if (item.isBlocked) {
      return i18n("Blocked");
    }
    if (item.errorCount > 0) {
      QString timeStr = item.lastTry.isValid()
                            ? item.lastTry.toString(QLocale::system().dateFormat(QLocale::ShortFormat))
                            : i18n("Unknown time");
      return i18n("Failed %1 times (Last: %2). Error: %3", item.errorCount, timeStr, item.lastError);
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
  int priority = item.requestData.value(QStringLiteral("priority")).toInt(0);
  int insertPos = calculateInsertPosition(priority);

  beginInsertRows(QModelIndex(), insertPos, insertPos);
  m_items.insert(insertPos, item);
  endInsertRows();

  save();
}

int QueueModel::calculateInsertPosition(int priority) const {
  for (int i = 0; i < m_items.size(); ++i) {
    const QueueItem &existingItem = m_items.at(i);
    int existingPriority = existingItem.requestData.value(QStringLiteral("priority")).toInt(0);
    if (priority > existingPriority) {
      return i;
    }
  }
  return m_items.size();
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
  save();
  return item;
}

QueueItem QueueModel::peek() const {
  if (m_items.isEmpty()) {
    return QueueItem();
  }
  return m_items.first();
}

void QueueModel::requeueFailed(const QueueItem &item, const QString &errorMsg, const QString &rawResponse) {
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

void QueueModel::moveItem(int from, int to) {
  if (from < 0 || from >= m_items.size() || to < 0 || to >= m_items.size() || from == to) {
    return;
  }

  int destinationChild = (to > from) ? to + 1 : to;

  if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), destinationChild)) {
    if (from < to)
      std::rotate(m_items.begin() + from, m_items.begin() + from + 1, m_items.begin() + to + 1);
    else
      std::rotate(m_items.begin() + to, m_items.begin() + from, m_items.begin() + from + 1);
    endMoveRows();
    save();
  }
}

bool QueueModel::isEmpty() const { return m_items.isEmpty(); }

int QueueModel::size() const { return m_items.size(); }

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

void QueueModel::clear() {
  if (!m_items.isEmpty()) {
    beginResetModel();
    m_items.clear();
    endResetModel();
    save();
  }
}

QString QueueModel::filePath() const {
  if (QFileInfo(m_filename).isAbsolute()) {
    return m_filename;
  }
  QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return dirPath + QStringLiteral("/") + m_filename;
}

void QueueModel::load() {
  QString path = filePath();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  QByteArray data = file.readAll();
  QJsonDocument doc(QJsonDocument::fromJson(data));
  QJsonArray arr;

  if (doc.isObject()) {
    QJsonObject topObj = doc.object();
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

void QueueModel::beginBatchUpdate() { m_batchUpdating = true; }

void QueueModel::endBatchUpdate() {
  m_batchUpdating = false;
  save();
}

void QueueModel::save() {
  if (m_batchUpdating)
    return;

  QString path = filePath();
  QFileInfo fileInfo(path);
  QDir dir = fileInfo.dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open queue.json for writing:" << file.errorString();
    return;
  }

  file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

  QJsonArray arr;
  for (const QueueItem &item : std::as_const(m_items)) {
    arr.append(item.toJson());
  }

  QJsonObject topObj;

  QJsonArray tsArr;
  for (const QDateTime &dt : std::as_const(m_runTimestamps)) {
    tsArr.append(dt.toString(Qt::ISODate));
  }
  topObj[QStringLiteral("m_runTimestamps")] = tsArr;
  topObj[QStringLiteral("items")] = arr;

  QJsonDocument doc(topObj);
  file.write(doc.toJson());
}
