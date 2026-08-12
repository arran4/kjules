#include "sessionrequestbuilder.h"

namespace SessionRequestBuilder {

QJsonObject createSession(const QJsonObject &requestData) {
  const QJsonObject nestedRequest = requestData.value(QStringLiteral("request")).toObject();
  const QJsonObject &input = nestedRequest.isEmpty() ? requestData : nestedRequest;
  QJsonObject request;
  request[QStringLiteral("prompt")] = input.value(QStringLiteral("prompt")).toString();

  const QString title = input.value(QStringLiteral("title")).toString();
  if (!title.isEmpty()) {
    request[QStringLiteral("title")] = title;
  }

  const QJsonObject suppliedSourceContext = input.value(QStringLiteral("sourceContext")).toObject();
  QString source = input.value(QStringLiteral("source")).toString();
  if (source.isEmpty()) {
    source = suppliedSourceContext.value(QStringLiteral("source")).toString();
  }
  if (!source.isEmpty()) {
    QString apiSource = source;
    if (apiSource.startsWith(QStringLiteral("sources/"))) {
      apiSource = apiSource.mid(8);
    }
    if (apiSource.startsWith(QStringLiteral("github/"))) {
      apiSource = apiSource.mid(7);
    }
    apiSource = QStringLiteral("sources/github/") + apiSource;

    QJsonObject sourceContext{{QStringLiteral("source"), apiSource}};
    QString startingBranch = input.value(QStringLiteral("startingBranch")).toString();
    if (startingBranch.isEmpty()) {
      startingBranch = suppliedSourceContext.value(QStringLiteral("githubRepoContext"))
                           .toObject()
                           .value(QStringLiteral("startingBranch"))
                           .toString();
    }
    if (!startingBranch.isEmpty()) {
      sourceContext[QStringLiteral("githubRepoContext")] =
          QJsonObject{{QStringLiteral("startingBranch"), startingBranch}};
    }
    request[QStringLiteral("sourceContext")] = sourceContext;
  }

  if (input.contains(QStringLiteral("requirePlanApproval"))) {
    request[QStringLiteral("requirePlanApproval")] = input.value(QStringLiteral("requirePlanApproval")).toBool();
  }

  const QString automationMode = input.value(QStringLiteral("automationMode")).toString();
  if (!automationMode.isEmpty()) {
    request[QStringLiteral("automationMode")] = automationMode;
  }

  return request;
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
