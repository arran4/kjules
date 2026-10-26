#include "sessionrequestbuilder.h"
#include "jobstore.h"

namespace SessionRequestBuilder {

QJsonObject createSession(const QJsonObject &requestData) {
  const QJsonObject input = normalizeSessionRequest(requestData);
  QJsonObject request;
  request[QStringLiteral("prompt")] = input.value(QStringLiteral("prompt")).toString();

  const QString title = input.value(QStringLiteral("title")).toString();
  if (!title.isEmpty()) {
    request[QStringLiteral("title")] = title;
  }

  const QString source = input.value(QStringLiteral("source")).toString();
  if (!source.isEmpty()) {
    QJsonObject sourceContext{{QStringLiteral("source"), source}};
    const QString startingBranch = input.value(QStringLiteral("startingBranch")).toString();
    if (!startingBranch.isEmpty()) {
      sourceContext[QStringLiteral("githubRepoContext")] =
          QJsonObject{{QStringLiteral("startingBranch"), startingBranch}};
    }
    request[QStringLiteral("sourceContext")] = sourceContext;
  }

  if (input.contains(QStringLiteral("requirePlanApproval"))) {
    request[QStringLiteral("requirePlanApproval")] = input.value(QStringLiteral("requirePlanApproval")).toBool();
  } else if (input.contains(QStringLiteral("planApproval"))) {
    request[QStringLiteral("requirePlanApproval")] = input.value(QStringLiteral("planApproval")).toBool();
  }

  const QString automationMode = input.value(QStringLiteral("automationMode")).toString();
  if (!automationMode.isEmpty()) {
    request[QStringLiteral("automationMode")] = automationMode;
  }

  return request;
}

QJsonObject buildSessionRequest(const QString &source, const QString &startingBranch, const QString &prompt,
                                const QString &automationMode, bool requirePlanApproval, bool ignoreConcurrency,
                                int priority, const QString &queueAction) {
  QJsonObject req;
  if (!source.isEmpty()) {
    req[QStringLiteral("source")] = source;
  }
  if (!startingBranch.isEmpty()) {
    req[QStringLiteral("startingBranch")] = startingBranch;
  }
  req[QStringLiteral("prompt")] = prompt;
  if (requirePlanApproval) {
    req[QStringLiteral("requirePlanApproval")] = true;
  }
  if (ignoreConcurrency) {
    req[QStringLiteral("ignoreConcurrency")] = true;
  }
  if (priority != 0) {
    req[QStringLiteral("priority")] = priority;
  }
  if (!automationMode.isEmpty()) {
    req[QStringLiteral("automationMode")] = automationMode;
  }
  if (!queueAction.isEmpty()) {
    req[QStringLiteral("_kjules_action")] = queueAction;
  }
  return req;
}

QJsonObject normalizeSessionRequest(const QJsonObject &requestData) {
  const QJsonObject nestedRequest = requestData.value(QStringLiteral("request")).toObject();
  const QJsonObject &input = nestedRequest.isEmpty() ? requestData : nestedRequest;

  QJsonObject req = input;

  if (input.contains(QStringLiteral("planApproval")) && !input.contains(QStringLiteral("requirePlanApproval"))) {
    req[QStringLiteral("requirePlanApproval")] = input.value(QStringLiteral("planApproval")).toBool();
  }

  if (input.contains(QStringLiteral("queueAction")) && !input.contains(QStringLiteral("_kjules_action"))) {
    req[QStringLiteral("_kjules_action")] = input.value(QStringLiteral("queueAction")).toString();
  }

  const auto sc = input.value(QStringLiteral("sourceContext")).toObject();
  if (req.value(QStringLiteral("source")).toString().isEmpty())
    req[QStringLiteral("source")] = sc.value(QStringLiteral("source"));
  if (req.value(QStringLiteral("startingBranch")).toString().isEmpty())
    req[QStringLiteral("startingBranch")] =
        sc.value(QStringLiteral("githubRepoContext")).toObject().value(QStringLiteral("startingBranch"));

  return req;
}

QJsonObject copyableSessionRequest(const QJsonObject &sessionOrRequest, const JobStore *jobStore) {
  QJsonObject baseRequest;
  const JobData *foundJob = nullptr;

  if (jobStore) {
    QString jobId = sessionOrRequest.value(QStringLiteral("_kjules_job_id")).toString();
    if (jobId.isEmpty()) {
      jobId = sessionOrRequest.value(QStringLiteral("jobId")).toString();
    }
    if (jobId.isEmpty()) {
      jobId = sessionOrRequest.value(QStringLiteral("id")).toString();
    }

    if (!jobId.isEmpty()) {
      foundJob = jobStore->getJobById(jobId);
      if (!foundJob) {
        foundJob = jobStore->getJobByAttemptId(jobId);
      }
      if (!foundJob) {
        foundJob = jobStore->getJobBySessionId(jobId);
      }
    }
  }

  QString requestedAttemptId = sessionOrRequest.value(QStringLiteral("attemptId")).toString();

  if (foundJob) {
    if (!requestedAttemptId.isEmpty()) {
      for (const auto &attempt : foundJob->attempts) {
        if (attempt.id == requestedAttemptId && !attempt.requestSnapshot.isEmpty()) {
          baseRequest = attempt.requestSnapshot;
          break;
        }
      }
    }
    if (baseRequest.isEmpty() && !foundJob->acceptedAttemptId.isEmpty()) {
      for (const auto &attempt : foundJob->attempts) {
        if (attempt.id == foundJob->acceptedAttemptId && !attempt.requestSnapshot.isEmpty()) {
          baseRequest = attempt.requestSnapshot;
          break;
        }
      }
    }
    if (baseRequest.isEmpty()) {
      for (int i = foundJob->attempts.size() - 1; i >= 0; --i) {
        if (!foundJob->attempts[i].requestSnapshot.isEmpty()) {
          baseRequest = foundJob->attempts[i].requestSnapshot;
          break;
        }
      }
    }
    if (baseRequest.isEmpty() && !foundJob->canonicalRequest.isEmpty()) {
      baseRequest = foundJob->canonicalRequest;
    }
  }

  if (baseRequest.isEmpty()) {
    baseRequest = sessionOrRequest;
  }

  QJsonObject normalized = normalizeSessionRequest(baseRequest);

  if (foundJob) {
    if (normalized.value(QStringLiteral("source")).toString().isEmpty() && !foundJob->source.isEmpty()) {
      normalized[QStringLiteral("source")] = foundJob->source;
    }
    if (normalized.value(QStringLiteral("startingBranch")).toString().isEmpty() &&
        !foundJob->startingBranch.isEmpty()) {
      normalized[QStringLiteral("startingBranch")] = foundJob->startingBranch;
    }
    if (normalized.value(QStringLiteral("prompt")).toString().isEmpty() && !foundJob->prompt.isEmpty()) {
      normalized[QStringLiteral("prompt")] = foundJob->prompt;
    }
    if (normalized.value(QStringLiteral("automationMode")).toString().isEmpty() &&
        !foundJob->automationMode.isEmpty()) {
      normalized[QStringLiteral("automationMode")] = foundJob->automationMode;
    }
    if (!normalized.contains(QStringLiteral("requirePlanApproval")) &&
        !normalized.contains(QStringLiteral("planApproval")) && foundJob->planApproval) {
      normalized[QStringLiteral("requirePlanApproval")] = true;
    }
    if (!normalized.contains(QStringLiteral("ignoreConcurrency")) && foundJob->ignoreConcurrency) {
      normalized[QStringLiteral("ignoreConcurrency")] = true;
    }
    if (!normalized.contains(QStringLiteral("priority")) && foundJob->priority != 0) {
      normalized[QStringLiteral("priority")] = foundJob->priority;
    }
  }

  QString source = normalized.value(QStringLiteral("source")).toString();
  QString startingBranch = normalized.value(QStringLiteral("startingBranch")).toString();
  QString prompt = normalized.value(QStringLiteral("prompt")).toString();
  QString automationMode = normalized.value(QStringLiteral("automationMode")).toString();
  bool requirePlanApproval = false;
  if (normalized.contains(QStringLiteral("requirePlanApproval"))) {
    requirePlanApproval = normalized.value(QStringLiteral("requirePlanApproval")).toBool();
  } else if (normalized.contains(QStringLiteral("planApproval"))) {
    requirePlanApproval = normalized.value(QStringLiteral("planApproval")).toBool();
  }
  bool ignoreConcurrency = normalized.value(QStringLiteral("ignoreConcurrency")).toBool();
  int priority = normalized.value(QStringLiteral("priority")).toInt();

  QJsonObject copy;
  if (!prompt.isEmpty()) {
    copy[QStringLiteral("prompt")] = prompt;
  }
  if (!source.isEmpty()) {
    copy[QStringLiteral("source")] = source;
  }
  if (!startingBranch.isEmpty()) {
    copy[QStringLiteral("startingBranch")] = startingBranch;
  }
  if (!automationMode.isEmpty()) {
    copy[QStringLiteral("automationMode")] = automationMode;
  }
  if (requirePlanApproval) {
    copy[QStringLiteral("requirePlanApproval")] = true;
    copy[QStringLiteral("planApproval")] = true;
  }
  if (ignoreConcurrency) {
    copy[QStringLiteral("ignoreConcurrency")] = true;
  }
  if (priority != 0) {
    copy[QStringLiteral("priority")] = priority;
  }

  if (normalized.contains(QStringLiteral("sources"))) {
    copy[QStringLiteral("sources")] = normalized.value(QStringLiteral("sources"));
  }

  if (!source.isEmpty()) {
    QJsonObject sourceContext{{QStringLiteral("source"), source}};
    if (!startingBranch.isEmpty()) {
      sourceContext[QStringLiteral("githubRepoContext")] =
          QJsonObject{{QStringLiteral("startingBranch"), startingBranch}};
    }
    copy[QStringLiteral("sourceContext")] = sourceContext;
  }

  return copy;
}

QJsonObject sendMessage(const QString &prompt) { return QJsonObject{{QStringLiteral("prompt"), prompt}}; }

QJsonObject sessionResponseWithRequest(const QJsonObject &response, const QJsonObject &request) {
  QJsonObject result = response;
  result[QStringLiteral("request")] = request;
  if (!result.contains(QStringLiteral("sourceContext")) && request.contains(QStringLiteral("sourceContext"))) {
    result[QStringLiteral("sourceContext")] = request.value(QStringLiteral("sourceContext"));
  }
  return result;
}

} // namespace SessionRequestBuilder
