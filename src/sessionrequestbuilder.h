#ifndef SESSIONREQUESTBUILDER_H
#define SESSIONREQUESTBUILDER_H

#include <QJsonObject>
#include <QString>

namespace SessionRequestBuilder {

QJsonObject createSession(const QJsonObject &requestData);
QJsonObject buildSessionRequest(const QString &source, const QString &startingBranch, const QString &prompt,
                                const QString &automationMode, bool requirePlanApproval, bool ignoreConcurrency,
                                int priority, const QString &queueAction = QString());
QJsonObject normalizeSessionRequest(const QJsonObject &requestData);
QJsonObject sendMessage(const QString &prompt);
QJsonObject sessionResponseWithRequest(const QJsonObject &response, const QJsonObject &request);

} // namespace SessionRequestBuilder

#endif // SESSIONREQUESTBUILDER_H
