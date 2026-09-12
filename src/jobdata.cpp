#include "jobdata.h"
#include "sessionrequestbuilder.h"
#include <QUuid>

QJsonObject JobAttemptData::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("id")] = id;
  if (!julesSessionId.isEmpty()) {
    obj[QStringLiteral("julesSessionId")] = julesSessionId;
  }
  obj[QStringLiteral("requestSnapshot")] = requestSnapshot;
  obj[QStringLiteral("dispatchState")] = dispatchState;
  obj[QStringLiteral("julesState")] = julesState;
  if (createdAt.isValid())
    obj[QStringLiteral("createdAt")] = createdAt.toString(Qt::ISODate);
  if (updatedAt.isValid())
    obj[QStringLiteral("updatedAt")] = updatedAt.toString(Qt::ISODate);
  if (!launchErrors.isEmpty())
    obj[QStringLiteral("launchErrors")] = launchErrors;
  if (!rawResponse.isEmpty())
    obj[QStringLiteral("rawResponse")] = rawResponse;
  if (!prMetadata.isEmpty())
    obj[QStringLiteral("prMetadata")] = prMetadata;
  obj[QStringLiteral("isAccepted")] = isAccepted;
  return obj;
}

JobAttemptData JobAttemptData::fromJson(const QJsonObject &obj) {
  JobAttemptData data;
  data.id = obj[QStringLiteral("id")].toString();
  data.julesSessionId = obj[QStringLiteral("julesSessionId")].toString();
  data.requestSnapshot = obj[QStringLiteral("requestSnapshot")].toObject();
  data.dispatchState = obj[QStringLiteral("dispatchState")].toString();
  data.julesState = obj[QStringLiteral("julesState")].toString();
  data.createdAt = QDateTime::fromString(obj[QStringLiteral("createdAt")].toString(), Qt::ISODate);
  data.updatedAt = QDateTime::fromString(obj[QStringLiteral("updatedAt")].toString(), Qt::ISODate);
  data.launchErrors = obj[QStringLiteral("launchErrors")].toArray();
  data.rawResponse = obj[QStringLiteral("rawResponse")].toObject();
  data.prMetadata = obj[QStringLiteral("prMetadata")].toObject();
  data.isAccepted = obj[QStringLiteral("isAccepted")].toBool();
  return data;
}

QJsonObject JobData::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("id")] = id;
  obj[QStringLiteral("canonicalRequest")] = canonicalRequest;
  obj[QStringLiteral("source")] = source;
  obj[QStringLiteral("startingBranch")] = startingBranch;
  obj[QStringLiteral("prompt")] = prompt;
  obj[QStringLiteral("automationMode")] = automationMode;
  obj[QStringLiteral("planApproval")] = planApproval;
  obj[QStringLiteral("ignoreConcurrency")] = ignoreConcurrency;
  obj[QStringLiteral("priority")] = priority;
  if (createdAt.isValid())
    obj[QStringLiteral("createdAt")] = createdAt.toString(Qt::ISODate);
  if (updatedAt.isValid())
    obj[QStringLiteral("updatedAt")] = updatedAt.toString(Qt::ISODate);
  if (!lifecycleMetadata.isEmpty())
    obj[QStringLiteral("lifecycleMetadata")] = lifecycleMetadata;
  QJsonArray attemptsArray;
  for (const auto &attempt : attempts) {
    attemptsArray.append(attempt.toJson());
  }
  obj[QStringLiteral("attempts")] = attemptsArray;
  if (!acceptedAttemptId.isEmpty())
    obj[QStringLiteral("acceptedAttemptId")] = acceptedAttemptId;
  if (!legacyMetadata.isEmpty())
    obj[QStringLiteral("legacyMetadata")] = legacyMetadata;
  return obj;
}

JobData JobData::fromJson(const QJsonObject &obj) {
  JobData data;
  data.id = obj[QStringLiteral("id")].toString();
  data.canonicalRequest = obj[QStringLiteral("canonicalRequest")].toObject();
  data.source = obj[QStringLiteral("source")].toString();
  data.startingBranch = obj[QStringLiteral("startingBranch")].toString();
  data.prompt = obj[QStringLiteral("prompt")].toString();
  data.automationMode = obj[QStringLiteral("automationMode")].toString();
  data.planApproval = obj[QStringLiteral("planApproval")].toBool();
  data.ignoreConcurrency = obj[QStringLiteral("ignoreConcurrency")].toBool();
  data.priority = obj[QStringLiteral("priority")].toInt();
  data.createdAt = QDateTime::fromString(obj[QStringLiteral("createdAt")].toString(), Qt::ISODate);
  data.updatedAt = QDateTime::fromString(obj[QStringLiteral("updatedAt")].toString(), Qt::ISODate);
  data.lifecycleMetadata = obj[QStringLiteral("lifecycleMetadata")].toObject();
  QJsonArray attemptsArray = obj[QStringLiteral("attempts")].toArray();
  for (const auto &attemptVal : attemptsArray) {
    data.attempts.append(JobAttemptData::fromJson(attemptVal.toObject()));
  }
  data.acceptedAttemptId = obj[QStringLiteral("acceptedAttemptId")].toString();
  data.legacyMetadata = obj[QStringLiteral("legacyMetadata")].toObject();
  return data;
}

JobData JobData::fromRequest(const QJsonObject &request) {
  JobData job;
  const auto normalized = SessionRequestBuilder::normalizeSessionRequest(request);
  job.canonicalRequest = request;
  job.source = normalized.value(QStringLiteral("source")).toString();
  job.startingBranch = normalized.value(QStringLiteral("startingBranch")).toString();
  job.prompt = normalized.value(QStringLiteral("prompt")).toString();
  job.automationMode = normalized.value(QStringLiteral("automationMode")).toString();
  job.planApproval = normalized.value(QStringLiteral("requirePlanApproval")).toBool();
  job.ignoreConcurrency = normalized.value(QStringLiteral("ignoreConcurrency")).toBool();
  job.priority = normalized.value(QStringLiteral("priority")).toInt();
  job.createdAt = QDateTime::currentDateTimeUtc();
  job.updatedAt = job.createdAt;
  job.recordHistory(QStringLiteral("queued"), QStringLiteral("Scheduled for launch"));
  return job;
}

void JobData::recordHistory(const QString &event, const QString &message) {
  auto history = lifecycleMetadata.value(QStringLiteral("history")).toArray();
  history.append(QJsonObject{{QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                             {QStringLiteral("event"), event},
                             {QStringLiteral("message"), message}});
  lifecycleMetadata[QStringLiteral("history")] = history;
}
