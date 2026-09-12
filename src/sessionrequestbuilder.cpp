#include "sessionrequestbuilder.h"

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
