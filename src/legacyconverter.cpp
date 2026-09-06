#include "legacyconverter.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

namespace {
QString canonicalizeJson(const QJsonObject &obj) {
  QJsonDocument doc(obj);
  return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QString deterministicUuid(const QString &domain, const QJsonObject &legacyMetadata, int duplicateIndex = 0) {
  QUuid nsUuid = QUuid::createUuidV5(QUuid(), domain);
  QString canonical = canonicalizeJson(legacyMetadata) + QStringLiteral("_%1").arg(duplicateIndex);
  return QUuid::createUuidV5(nsUuid, canonical).toString(QUuid::WithoutBraces);
}
} // namespace

ConversionResult LegacyConverter::convertAll(const LegacyData &data, const QDateTime &fallbackTimestamp) {
  ConversionResult result;
  QVector<JobData> allJobs;

  QMap<QString, QVector<JobData>> groupedJobs;

  // Convert Queue
  for (int i = 0; i < data.queueItems.size(); ++i) {
    allJobs.append(fromQueueItem(data.queueItems[i], false, fallbackTimestamp, i));
  }

  // Convert Holding
  for (int i = 0; i < data.holdingItems.size(); ++i) {
    allJobs.append(fromQueueItem(data.holdingItems[i], true, fallbackTimestamp, i));
  }

  // Convert Active Sessions
  for (int i = 0; i < data.activeSessions.size(); ++i) {
    JobData job = fromSession(data.activeSessions[i].toObject(), false, fallbackTimestamp, i);
    if (job.legacyMetadata.contains(QStringLiteral("previousAttemptId"))) {
      QString prevId = job.legacyMetadata[QStringLiteral("previousAttemptId")].toString();
      groupedJobs[prevId].append(job);
    } else {
      allJobs.append(job);
    }
  }

  // Convert Archived Sessions
  for (int i = 0; i < data.archivedSessions.size(); ++i) {
    JobData job = fromSession(data.archivedSessions[i].toObject(), true, fallbackTimestamp, i);
    if (job.legacyMetadata.contains(QStringLiteral("previousAttemptId"))) {
      QString prevId = job.legacyMetadata[QStringLiteral("previousAttemptId")].toString();
      groupedJobs[prevId].append(job);
    } else {
      allJobs.append(job);
    }
  }

  // Convert Errors
  for (int i = 0; i < data.errors.size(); ++i) {
    QJsonObject errorObj = data.errors[i].toObject();
    if (errorObj.contains(QStringLiteral("request")) && errorObj[QStringLiteral("request")].isObject()) {
      allJobs.append(fromError(errorObj, fallbackTimestamp, i));
    } else {
      result.unattachedErrors.append(errorObj);
    }
  }

  // Process groupings
  for (auto it = groupedJobs.begin(); it != groupedJobs.end(); ++it) {
    QString prevId = it.key();
    QVector<JobData> group = it.value();

    bool foundParent = false;
    for (JobData &mainJob : allJobs) {
      if (!mainJob.attempts.isEmpty() && mainJob.attempts.last().julesSessionId == prevId) {
        // Merge
        for (const JobData &groupedJob : group) {
          mainJob.attempts.append(groupedJob.attempts);
        }
        foundParent = true;
        break;
      }
    }

    if (!foundParent) {
      allJobs.append(group);
    }
  }

  result.jobs = allJobs;
  return result;
}

QVector<JobData> LegacyConverter::convertQueue(const QVector<QueueItem> &items, bool isHolding,
                                               const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  for (int i = 0; i < items.size(); ++i) {
    jobs.append(fromQueueItem(items[i], isHolding, fallbackTimestamp, i));
  }
  return jobs;
}

QVector<JobData> LegacyConverter::convertSessions(const QJsonArray &sessions, bool isArchive,
                                                  const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  for (int i = 0; i < sessions.size(); ++i) {
    jobs.append(fromSession(sessions[i].toObject(), isArchive, fallbackTimestamp, i));
  }
  return jobs;
}

QVector<JobData> LegacyConverter::convertErrors(const QJsonArray &errors, const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  for (int i = 0; i < errors.size(); ++i) {
    jobs.append(fromError(errors[i].toObject(), fallbackTimestamp, i));
  }
  return jobs;
}

JobData LegacyConverter::fromQueueItem(const QueueItem &item, bool isHolding, const QDateTime &fallbackTimestamp,
                                       int duplicateIndex) {
  JobData job;
  QJsonObject legacy;
  legacy[QStringLiteral("errorCount")] = item.errorCount;
  legacy[QStringLiteral("lastError")] = item.lastError;
  legacy[QStringLiteral("lastResponse")] = item.lastResponse;
  if (item.lastTry.isValid()) {
    legacy[QStringLiteral("lastTry")] = item.lastTry.toString(Qt::ISODate);
  }
  legacy[QStringLiteral("pastErrors")] = item.pastErrors;
  job.legacyMetadata = legacy;

  QJsonObject contextObj = item.requestData;
  contextObj[QStringLiteral("_provenance")] = isHolding ? QStringLiteral("holding") : QStringLiteral("queue");
  job.id = deterministicUuid(QStringLiteral("queue-job-v1"), contextObj, duplicateIndex);
  job.canonicalRequest = item.requestData;

  job.source = item.requestData[QStringLiteral("source")].toString();
  if (job.source.isEmpty() && item.requestData.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sourceCtx = item.requestData[QStringLiteral("sourceContext")].toObject();
    job.source = sourceCtx[QStringLiteral("source")].toString();
  }

  job.prompt = item.requestData[QStringLiteral("prompt")].toString();

  job.createdAt = fallbackTimestamp;
  job.updatedAt = fallbackTimestamp;

  QJsonObject lifecycle;
  lifecycle[QStringLiteral("isHolding")] = isHolding;
  lifecycle[QStringLiteral("isBlocked")] = item.isBlocked;
  if (!item.blockMetadata.isEmpty()) {
    lifecycle[QStringLiteral("blockMetadata")] = item.blockMetadata;
  }
  job.lifecycleMetadata = lifecycle;
  return job;
}

JobData LegacyConverter::fromSession(const QJsonObject &session, bool isArchive, const QDateTime &fallbackTimestamp,
                                     int duplicateIndex) {
  JobData job;
  job.legacyMetadata = session; // Store all original session data
  job.id = deterministicUuid(QStringLiteral("session-job-v1"), session, duplicateIndex);

  job.source = session[QStringLiteral("source")].toString();
  if (job.source.isEmpty() && session.contains(QStringLiteral("sourceContext"))) {
    job.source = session[QStringLiteral("sourceContext")].toObject()[QStringLiteral("source")].toString();
  }

  job.prompt = session[QStringLiteral("prompt")].toString();
  job.createdAt = QDateTime::fromString(session[QStringLiteral("createTime")].toString(), Qt::ISODate);
  if (!job.createdAt.isValid())
    job.createdAt = fallbackTimestamp;
  job.updatedAt = QDateTime::fromString(session[QStringLiteral("updateTime")].toString(), Qt::ISODate);
  if (!job.updatedAt.isValid())
    job.updatedAt = fallbackTimestamp;

  QJsonObject lifecycle;
  lifecycle[QStringLiteral("isArchive")] = isArchive;
  if (session.contains(QStringLiteral("local_favourite")))
    lifecycle[QStringLiteral("favouriteRank")] = session[QStringLiteral("local_favourite")].toInt();
  if (session.contains(QStringLiteral("local_snooze_until")))
    lifecycle[QStringLiteral("snoozeUntil")] = session[QStringLiteral("local_snooze_until")].toString();
  if (session.contains(QStringLiteral("local_refreshInterval")))
    lifecycle[QStringLiteral("refreshInterval")] = session[QStringLiteral("local_refreshInterval")].toInt();
  job.lifecycleMetadata = lifecycle;

  JobAttemptData attempt;
  attempt.id = deterministicUuid(QStringLiteral("session-attempt-v1"), session, duplicateIndex);
  attempt.julesSessionId = session[QStringLiteral("id")].toString();
  attempt.julesState = session[QStringLiteral("state")].toString();
  attempt.createdAt = job.createdAt;
  attempt.updatedAt = job.updatedAt;

  // Set canonicalRequest based on rawObject if available
  if (session.contains(QStringLiteral("rawObject"))) {
    attempt.rawResponse = session[QStringLiteral("rawObject")].toObject();
    attempt.requestSnapshot = attempt.rawResponse[QStringLiteral("request")].toObject();
    if (job.canonicalRequest.isEmpty())
      job.canonicalRequest = attempt.requestSnapshot;
  }
  if (job.canonicalRequest.isEmpty()) {
    job.canonicalRequest = session[QStringLiteral("request")].toObject();
    attempt.requestSnapshot = job.canonicalRequest;
  }

  job.attempts.append(attempt);
  return job;
}

JobData LegacyConverter::fromError(const QJsonObject &error, const QDateTime &fallbackTimestamp, int duplicateIndex) {
  JobData job;
  job.legacyMetadata = error;
  job.id = deterministicUuid(QStringLiteral("error-job-v1"), error, duplicateIndex);

  job.canonicalRequest = error[QStringLiteral("request")].toObject();

  if (job.canonicalRequest.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sourceCtx = job.canonicalRequest[QStringLiteral("sourceContext")].toObject();
    job.source = sourceCtx[QStringLiteral("source")].toString();
  }
  job.prompt = job.canonicalRequest[QStringLiteral("prompt")].toString();

  job.createdAt = QDateTime::fromString(error[QStringLiteral("timestamp")].toString(), Qt::ISODate);
  if (!job.createdAt.isValid()) {
    job.createdAt = fallbackTimestamp;
  }
  job.updatedAt = job.createdAt;

  JobAttemptData attempt;
  attempt.id = deterministicUuid(QStringLiteral("error-attempt-v1"), error, duplicateIndex);
  attempt.requestSnapshot = job.canonicalRequest;
  attempt.dispatchState = QStringLiteral("FAILED");
  attempt.createdAt = job.createdAt;
  attempt.updatedAt = job.updatedAt;

  QJsonObject errResp;
  errResp[QStringLiteral("message")] = error[QStringLiteral("message")].toString();
  errResp[QStringLiteral("httpDetails")] = error[QStringLiteral("httpDetails")].toString();
  errResp[QStringLiteral("response")] = error[QStringLiteral("response")].toObject();

  QJsonArray errors;
  errors.append(errResp);
  attempt.launchErrors = errors;

  job.attempts.append(attempt);
  return job;
}
