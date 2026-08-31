#include "legacydatarepair.h"
#include "queuemodel.h"
#include "sessionmodel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStandardPaths>

QString LegacyDataRepair::getLegacyDataPath() const {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/org.kde.kjules");
}

QString LegacyDataRepair::getCurrentDataPath() const {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QJsonArray LegacyDataRepair::parseLegacySessions(const QString &path, bool &outIsMalformed) const {
  outIsMalformed = false;
  QFile file(path);
  if (!file.exists()) {
    return {};
  }
  if (!file.open(QIODevice::ReadOnly)) {
    outIsMalformed = true;
    return {};
  }
  QByteArray data = file.readAll();
  if (data.isEmpty()) {
    return {};
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError) {
    outIsMalformed = true;
    return {};
  }

  if (doc.isArray()) {
    return doc.array();
  } else if (doc.isObject()) {
    QJsonValue sessionsValue = doc.object().value(QStringLiteral("sessions"));
    if (!sessionsValue.isArray()) {
      outIsMalformed = true;
      return {};
    }
    return sessionsValue.toArray();
  }

  outIsMalformed = true;
  return {};
}

QJsonArray LegacyDataRepair::parseLegacyQueue(const QString &path, bool &outIsMalformed) const {
  outIsMalformed = false;
  QFile file(path);
  if (!file.exists()) {
    return {};
  }
  if (!file.open(QIODevice::ReadOnly)) {
    outIsMalformed = true;
    return {};
  }
  QByteArray data = file.readAll();
  if (data.isEmpty()) {
    return {};
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError) {
    outIsMalformed = true;
    return {};
  }

  if (doc.isArray()) {
    return doc.array();
  } else if (doc.isObject()) {
    QJsonValue itemsValue = doc.object().value(QStringLiteral("items"));
    if (!itemsValue.isArray()) {
      outIsMalformed = true;
      return {};
    }
    return itemsValue.toArray();
  }

  outIsMalformed = true;
  return {};
}

bool LegacyDataRepair::backupCurrentFiles(const QString &currentDataPath, QString &errorOut) const {
  QDir dir(currentDataPath);

  bool hasQueue = QFile::exists(dir.absoluteFilePath(QStringLiteral("queue.json")));
  bool hasSessions = QFile::exists(dir.absoluteFilePath(QStringLiteral("cached_all_sessions.json")));

  if (!hasQueue && !hasSessions) {
    return true; // Nothing to backup
  }

  QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  QString backupDirName = QStringLiteral("repair-backups/legacy-merge-%1").arg(timestamp);

  if (!dir.mkpath(backupDirName)) {
    errorOut = QStringLiteral("Failed to create backup directory.");
    return false;
  }

  QDir backupDir(dir.absoluteFilePath(backupDirName));

  if (hasQueue) {
    if (!QFile::copy(dir.absoluteFilePath(QStringLiteral("queue.json")),
                     backupDir.absoluteFilePath(QStringLiteral("queue.json")))) {
      errorOut = QStringLiteral("Failed to backup current queue.json.");
      return false;
    }
  }

  if (hasSessions) {
    if (!QFile::copy(dir.absoluteFilePath(QStringLiteral("cached_all_sessions.json")),
                     backupDir.absoluteFilePath(QStringLiteral("cached_all_sessions.json")))) {
      errorOut = QStringLiteral("Failed to backup current cached_all_sessions.json.");
      return false;
    }
  }

  return true;
}

LegacyDataRepairResult LegacyDataRepair::analyze(SessionModel *sessionModel, QueueModel *queueModel) {
  LegacyDataRepairResult result;

  QString legacyPath = getLegacyDataPath();

  bool sessionsMalformed = false;
  QJsonArray legacySessions =
      parseLegacySessions(legacyPath + QStringLiteral("/cached_all_sessions.json"), sessionsMalformed);

  bool queueMalformed = false;
  QJsonArray legacyQueue = parseLegacyQueue(legacyPath + QStringLiteral("/queue.json"), queueMalformed);

  if (sessionsMalformed) {
    result.error += QStringLiteral("Legacy Following file is malformed or unreadable.\n");
  }
  if (queueMalformed) {
    result.error += QStringLiteral("Legacy Queue file is malformed or unreadable.\n");
  }

  result.legacyFollowingCount = legacySessions.size();
  result.legacyQueueCount = legacyQueue.size();

  m_cachedSessionsToRecover = QJsonArray();
  m_cachedQueueToRecover = QJsonArray();

  if (sessionModel && !sessionsMalformed) {
    result.currentFollowingCount = sessionModel->rowCount();
    QSet<QString> currentIds;
    for (int i = 0; i < sessionModel->rowCount(); ++i) {
      currentIds.insert(sessionModel->data(sessionModel->index(i, 0), SessionModel::IdRole).toString());
    }

    for (const QJsonValue &val : legacySessions) {
      if (!val.isObject()) {
        result.followingSkippedInvalid++;
        continue;
      }
      QJsonObject obj = val.toObject();
      QString id = obj.value(QStringLiteral("id")).toString();
      if (id.isEmpty()) {
        result.followingSkippedInvalid++;
        continue;
      }

      if (currentIds.contains(id)) {
        result.followingAlreadyPresent++;
      } else {
        m_cachedSessionsToRecover.append(obj);
        currentIds.insert(id); // Avoid recovering same legacy ID multiple times
        result.followingToRecover++;
      }
    }
  }

  if (queueModel && !queueMalformed) {
    result.currentQueueCount = queueModel->size();
    QVector<bool> matched(queueModel->size(), false);

    for (const QJsonValue &val : legacyQueue) {
      if (!val.isObject()) {
        result.queueSkippedInvalid++;
        continue;
      }
      QJsonObject legacyObj = val.toObject();
      if (!legacyObj.contains(QStringLiteral("requestData"))) {
        result.queueSkippedInvalid++;
        continue;
      }
      QJsonObject legacyReqData = legacyObj.value(QStringLiteral("requestData")).toObject();
      if (legacyReqData.isEmpty()) {
        result.queueSkippedInvalid++;
        continue;
      }

      bool found = false;
      for (int i = 0; i < queueModel->size(); ++i) {
        if (matched[i])
          continue;
        QueueItem currentItem = queueModel->getItem(i);
        if (currentItem.requestData == legacyReqData) {
          matched[i] = true;
          found = true;
          break;
        }
      }

      if (found) {
        result.queueAlreadyPresent++;
      } else {
        m_cachedQueueToRecover.append(legacyObj);
        result.queueToRecover++;
      }
    }
  }

  return result;
}

LegacyDataRepairResult LegacyDataRepair::performMerge(SessionModel *sessionModel, QueueModel *queueModel) {
  // Re-analyze against live models to make sure counts and states are current right before mutating
  LegacyDataRepairResult result = analyze(sessionModel, queueModel);

  if (result.followingToRecover == 0 && result.queueToRecover == 0) {
    return result;
  }

  QString errorOut;
  if (!backupCurrentFiles(getCurrentDataPath(), errorOut)) {
    result.fatalError = errorOut;
    return result;
  }

  if (sessionModel && result.followingToRecover > 0) {
    sessionModel->addSessions(m_cachedSessionsToRecover);
    sessionModel->saveSessions();
  }

  if (queueModel && result.queueToRecover > 0) {
    queueModel->beginBatchUpdate();
    for (const QJsonValue &val : m_cachedQueueToRecover) {
      QueueItem item = QueueItem::fromJson(val.toObject());
      queueModel->insertItem(queueModel->size(), item); // strict append, no priority sorting
    }
    queueModel->endBatchUpdate();
  }

  return result;
}
