#ifndef SESSIONREQUESTBUILDER_H
#define SESSIONREQUESTBUILDER_H

#include <QJsonObject>
#include <QString>

namespace SessionRequestBuilder {

QJsonObject createSession(const QJsonObject &requestData);
QJsonObject sendMessage(const QString &prompt);
QJsonObject sessionResponseWithRequest(const QJsonObject &response, const QJsonObject &request);

} // namespace SessionRequestBuilder

#endif // SESSIONREQUESTBUILDER_H
