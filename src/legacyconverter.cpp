#include "legacyconverter.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

namespace {
QString canonicalizeJson(const QJsonObject &obj) {
  QJsonDocument doc(obj);
  return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QString deterministicUuid(const QString &domain, const QJsonObject &legacyMetadata, int ordinal = 0) {
  QUuid nsUuid = QUuid::createUuidV5(QUuid(), domain);
  QString canonical = canonicalizeJson(legacyMetadata) + QStringLiteral("_%1").arg(ordinal);
  return QUuid::createUuidV5(nsUuid, canonical).toString(QUuid::WithoutBraces);
}
} // namespace

ConversionResult LegacyConverter::convertAll(const LegacyData &data, const QDateTime &fallbackTimestamp) {
  ConversionResult result;
  QVector<JobData> allJobs;
  QMap<QString, QVector<JobData>> groupedJobs;

  auto extractPrevId = [](const QJsonObject &sessionObj) -> QString {
    if (sessionObj.contains(QStringLiteral("previousAttemptId"))) {
      return sessionObj[QStringLiteral("previousAttemptId")].toString();
    }
    if (sessionObj.contains(QStringLiteral("request"))) {
      QJsonObject req = sessionObj[QStringLiteral("request")].toObject();
      if (req.contains(QStringLiteral("previousAttemptId"))) {
        return req[QStringLiteral("previousAttemptId")].toString();
      }
    }
    return QString();
  };

  QMap<QString, int> queueCounts;
  for (int i = 0; i < data.queueItems.size(); ++i) {
    QString canonical = canonicalizeJson(data.queueItems[i].requestData);
    int ordinal = queueCounts[canonical]++;
    allJobs.append(fromQueueItem(data.queueItems[i], false, fallbackTimestamp, ordinal));
  }

  QMap<QString, int> holdingCounts;
  for (int i = 0; i < data.holdingItems.size(); ++i) {
    QString canonical = canonicalizeJson(data.holdingItems[i].requestData);
    int ordinal = holdingCounts[canonical]++;
    allJobs.append(fromQueueItem(data.holdingItems[i], true, fallbackTimestamp, ordinal));
  }

  QMap<QString, int> activeCounts;
  for (int i = 0; i < data.activeSessions.size(); ++i) {
    QJsonObject sessionObj = data.activeSessions[i].toObject();
    QString canonical = canonicalizeJson(sessionObj);
    int ordinal = activeCounts[canonical]++;
    JobData job = fromSession(sessionObj, false, fallbackTimestamp, ordinal);
    QString prevId = extractPrevId(sessionObj);
    if (!prevId.isEmpty()) {
      groupedJobs[prevId].append(job);
    } else {
      allJobs.append(job);
    }
  }

  QMap<QString, int> archivedCounts;
  for (int i = 0; i < data.archivedSessions.size(); ++i) {
    QJsonObject sessionObj = data.archivedSessions[i].toObject();
    QString canonical = canonicalizeJson(sessionObj);
    int ordinal = archivedCounts[canonical]++;
    JobData job = fromSession(sessionObj, true, fallbackTimestamp, ordinal);
    QString prevId = extractPrevId(sessionObj);
    if (!prevId.isEmpty()) {
      groupedJobs[prevId].append(job);
    } else {
      allJobs.append(job);
    }
  }

  QMap<QString, int> errorCounts;
  for (int i = 0; i < data.errors.size(); ++i) {
    QJsonObject errorObj = data.errors[i].toObject();
    if (errorObj.contains(QStringLiteral("request")) && errorObj[QStringLiteral("request")].isObject()) {

      QJsonObject req = errorObj[QStringLiteral("request")].toObject();
      // Does it look like logical work? Need sourceContext or prompt
      if (req.contains(QStringLiteral("sourceContext")) || req.contains(QStringLiteral("prompt"))) {
        QString canonical = canonicalizeJson(errorObj);
        int ordinal = errorCounts[canonical]++;

        QString sessionId;
        if (errorObj.contains(QStringLiteral("sessionId"))) {
          sessionId = errorObj[QStringLiteral("sessionId")].toString();
        }

        if (!sessionId.isEmpty()) {
          // Attach to session attempt
          bool found = false;
          for (JobData &job : allJobs) {
            for (JobAttemptData &attempt : job.attempts) {
              if (attempt.julesSessionId == sessionId) {
                QJsonObject errResp;
                errResp[QStringLiteral("message")] = errorObj[QStringLiteral("message")].toString();
                errResp[QStringLiteral("httpDetails")] = errorObj[QStringLiteral("httpDetails")].toString();
                errResp[QStringLiteral("response")] = errorObj[QStringLiteral("response")].toObject();
                attempt.launchErrors.append(errResp);
                found = true;
                break;
              }
            }
            if (found)
              break;
          }
          if (!found) {
            result.unattachedErrors.append(errorObj);
          }
        } else {
          allJobs.append(fromError(errorObj, fallbackTimestamp, ordinal));
        }
      } else {
        result.unattachedErrors.append(errorObj);
      }
    } else {
      result.unattachedErrors.append(errorObj);
    }
  }

  // Resolve groupedJobs correctly for multi-hop
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto it = groupedJobs.begin(); it != groupedJobs.end();) {
      QString prevId = it.key();
      QVector<JobData> group = it.value();

      bool foundParent = false;
      for (JobData &mainJob : allJobs) {
        if (!mainJob.attempts.isEmpty() && mainJob.attempts.last().julesSessionId == prevId) {
          for (const JobData &groupedJob : group) {
            mainJob.attempts.append(groupedJob.attempts);
          }
          foundParent = true;
          break;
        }
      }

      if (foundParent) {
        it = groupedJobs.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
  }

  // Any remaining grouped jobs couldn't be resolved, add as separate jobs
  for (auto it = groupedJobs.begin(); it != groupedJobs.end(); ++it) {
    allJobs.append(it.value());
  }

  result.jobs = allJobs;
  return result;
}

QVector<JobData> LegacyConverter::convertQueue(const QVector<QueueItem> &items, bool isHolding,
                                               const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  QMap<QString, int> counts;
  for (int i = 0; i < items.size(); ++i) {
    QString canonical = canonicalizeJson(items[i].requestData);
    int ordinal = counts[canonical]++;
    jobs.append(fromQueueItem(items[i], isHolding, fallbackTimestamp, ordinal));
  }
  return jobs;
}

QVector<JobData> LegacyConverter::convertSessions(const QJsonArray &sessions, bool isArchive,
                                                  const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  QMap<QString, int> counts;
  for (int i = 0; i < sessions.size(); ++i) {
    QString canonical = canonicalizeJson(sessions[i].toObject());
    int ordinal = counts[canonical]++;
    jobs.append(fromSession(sessions[i].toObject(), isArchive, fallbackTimestamp, ordinal));
  }
  return jobs;
}

QVector<JobData> LegacyConverter::convertErrors(const QJsonArray &errors, const QDateTime &fallbackTimestamp) {
  QVector<JobData> jobs;
  QMap<QString, int> counts;
  for (int i = 0; i < errors.size(); ++i) {
    QString canonical = canonicalizeJson(errors[i].toObject());
    int ordinal = counts[canonical]++;
    jobs.append(fromError(errors[i].toObject(), fallbackTimestamp, ordinal));
  }
  return jobs;
}

JobData LegacyConverter::fromQueueItem(const QueueItem &item, bool isHolding, const QDateTime &fallbackTimestamp,
                                       int ordinal) {
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
  job.id = deterministicUuid(QStringLiteral("queue-job-v1"), contextObj, ordinal);
  job.canonicalRequest = item.requestData;

  job.source = item.requestData[QStringLiteral("source")].toString();
  if (job.source.isEmpty() && item.requestData.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sourceCtx = item.requestData[QStringLiteral("sourceContext")].toObject();
    job.source = sourceCtx[QStringLiteral("source")].toString();

    if (sourceCtx.contains(QStringLiteral("githubRepoContext"))) {
      QJsonObject ghCtx = sourceCtx[QStringLiteral("githubRepoContext")].toObject();
      if (ghCtx.contains(QStringLiteral("startingBranch"))) {
        job.startingBranch = ghCtx[QStringLiteral("startingBranch")].toString();
      }
    }
  }

  job.prompt = item.requestData[QStringLiteral("prompt")].toString();

  if (item.requestData.contains(QStringLiteral("automationMode"))) {
    job.automationMode = item.requestData[QStringLiteral("automationMode")].toString();
  }
  if (item.requestData.contains(QStringLiteral("preferences"))) {
    QJsonObject prefs = item.requestData[QStringLiteral("preferences")].toObject();
    if (prefs.contains(QStringLiteral("planApproval"))) {
      job.planApproval = prefs[QStringLiteral("planApproval")].toBool();
    }
    if (prefs.contains(QStringLiteral("ignoreConcurrency"))) {
      job.ignoreConcurrency = prefs[QStringLiteral("ignoreConcurrency")].toBool();
    }
    if (prefs.contains(QStringLiteral("priority"))) {
      job.priority = prefs[QStringLiteral("priority")].toInt();
    }
  }

  // Also support top-level queue properties used by older flows
  if (item.requestData.contains(QStringLiteral("priority"))) {
    job.priority = item.requestData[QStringLiteral("priority")].toInt();
  }
  if (item.requestData.contains(QStringLiteral("requirePlanApproval"))) {
    job.planApproval = item.requestData[QStringLiteral("requirePlanApproval")].toBool();
  }

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
                                     int ordinal) {
  JobData job;
  job.legacyMetadata = session; // Store all original session data
  job.id = deterministicUuid(QStringLiteral("session-job-v1"), session, ordinal);

  job.source = session[QStringLiteral("source")].toString();
  if (job.source.isEmpty() && session.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sourceCtx = session[QStringLiteral("sourceContext")].toObject();
    job.source = sourceCtx[QStringLiteral("source")].toString();

    if (sourceCtx.contains(QStringLiteral("githubRepoContext"))) {
      QJsonObject ghCtx = sourceCtx[QStringLiteral("githubRepoContext")].toObject();
      if (ghCtx.contains(QStringLiteral("startingBranch"))) {
        job.startingBranch = ghCtx[QStringLiteral("startingBranch")].toString();
      }
    }
  }

  job.prompt = session[QStringLiteral("prompt")].toString();

  if (session.contains(QStringLiteral("automationMode"))) {
    job.automationMode = session[QStringLiteral("automationMode")].toString();
  } else if (session.contains(QStringLiteral("request")) &&
             session[QStringLiteral("request")].toObject().contains(QStringLiteral("automationMode"))) {
    job.automationMode = session[QStringLiteral("request")].toObject()[QStringLiteral("automationMode")].toString();
  }

  if (session.contains(QStringLiteral("createTime"))) {
    job.createdAt = QDateTime::fromString(session[QStringLiteral("createTime")].toString(), Qt::ISODate);
  }
  if (!job.createdAt.isValid())
    job.createdAt = fallbackTimestamp;

  if (session.contains(QStringLiteral("updateTime"))) {
    job.updatedAt = QDateTime::fromString(session[QStringLiteral("updateTime")].toString(), Qt::ISODate);
  }
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
  attempt.id = deterministicUuid(QStringLiteral("session-attempt-v1"), session, ordinal);
  attempt.julesSessionId = session[QStringLiteral("id")].toString();
  attempt.julesState = session[QStringLiteral("state")].toString();
  attempt.createdAt = job.createdAt;
  attempt.updatedAt = job.updatedAt;

  // Extract PR metadata accurately
  QJsonObject prMetadata;
  if (session.contains(QStringLiteral("githubPrInfo"))) {
    QJsonObject gh = session[QStringLiteral("githubPrInfo")].toObject();
    prMetadata[QStringLiteral("url")] = gh[QStringLiteral("url")];
    prMetadata[QStringLiteral("number")] = gh[QStringLiteral("number")];
    prMetadata[QStringLiteral("status")] = gh[QStringLiteral("status")];
    prMetadata[QStringLiteral("labels")] = gh[QStringLiteral("labels")];
  } else if (session.contains(QStringLiteral("pullRequest"))) {
    QJsonObject pr = session[QStringLiteral("pullRequest")].toObject();
    prMetadata[QStringLiteral("url")] = pr[QStringLiteral("url")];
    if (pr.contains(QStringLiteral("state"))) {
      prMetadata[QStringLiteral("status")] = pr[QStringLiteral("state")];
    }
  } else if (session.contains(QStringLiteral("outputs"))) {
    QJsonArray outputs = session[QStringLiteral("outputs")].toArray();
    for (const auto &out : outputs) {
      QJsonObject outObj = out.toObject();
      if (outObj.contains(QStringLiteral("pullRequest"))) {
        QJsonObject pr = outObj[QStringLiteral("pullRequest")].toObject();
        prMetadata[QStringLiteral("url")] = pr[QStringLiteral("url")];
        if (pr.contains(QStringLiteral("state"))) {
          prMetadata[QStringLiteral("status")] = pr[QStringLiteral("state")];
        }
        break;
      }
    }
  }
  if (!prMetadata.isEmpty()) {
    attempt.prMetadata = prMetadata;
  }

  // Set canonicalRequest based on rawObject if available
  if (session.contains(QStringLiteral("rawObject"))) {
    attempt.rawResponse = session[QStringLiteral("rawObject")].toObject();
    if (attempt.rawResponse.contains(QStringLiteral("request"))) {
      attempt.requestSnapshot = attempt.rawResponse[QStringLiteral("request")].toObject();
    }
  }

  if (attempt.requestSnapshot.isEmpty() && session.contains(QStringLiteral("request"))) {
    attempt.requestSnapshot = session[QStringLiteral("request")].toObject();
  }

  job.canonicalRequest = attempt.requestSnapshot;

  job.attempts.append(attempt);
  return job;
}

JobData LegacyConverter::fromError(const QJsonObject &error, const QDateTime &fallbackTimestamp, int ordinal) {
  JobData job;
  job.legacyMetadata = error;
  job.id = deterministicUuid(QStringLiteral("error-job-v1"), error, ordinal);

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
  attempt.id = deterministicUuid(QStringLiteral("error-attempt-v1"), error, ordinal);
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
