#include "apierror.h"
#include <QJsonArray>
#include <QJsonDocument>

ApiError::ApiError() : m_type(Type::Unknown), m_httpStatusCode(0) {}

ApiError::ApiError(Type type, const QString &message, int httpStatusCode)
    : m_type(type), m_message(message), m_httpStatusCode(httpStatusCode) {}

ApiError::Type ApiError::type() const { return m_type; }
void ApiError::setType(Type type) { m_type = type; }

QString ApiError::message() const { return m_message; }
void ApiError::setMessage(const QString &message) { m_message = message; }

QString ApiError::details() const { return m_details; }
void ApiError::setDetails(const QString &details) { m_details = details; }

int ApiError::httpStatusCode() const { return m_httpStatusCode; }
void ApiError::setHttpStatusCode(int code) { m_httpStatusCode = code; }

QJsonObject ApiError::rawResponse() const { return m_rawResponse; }
void ApiError::setRawResponse(const QJsonObject &response) { m_rawResponse = response; }

QJsonObject ApiError::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("type")] = static_cast<int>(m_type);
  obj[QStringLiteral("message")] = m_message;
  obj[QStringLiteral("details")] = m_details;
  obj[QStringLiteral("httpStatusCode")] = m_httpStatusCode;
  obj[QStringLiteral("rawResponse")] = m_rawResponse;
  return obj;
}

ApiError ApiError::fromJson(const QJsonObject &json) {
  ApiError error;
  error.setType(static_cast<Type>(json.value(QStringLiteral("type")).toInt()));
  error.setMessage(json.value(QStringLiteral("message")).toString());
  error.setDetails(json.value(QStringLiteral("details")).toString());
  error.setHttpStatusCode(json.value(QStringLiteral("httpStatusCode")).toInt());
  error.setRawResponse(json.value(QStringLiteral("rawResponse")).toObject());
  return error;
}

ApiError ApiErrorDetector::detect(QNetworkReply *reply, const QByteArray &responseData) {
  ApiError apiError;
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  apiError.setHttpStatusCode(statusCode);

  QByteArray data = responseData;
  if (data.isEmpty() && reply->bytesAvailable() > 0 && reply->isReadable()) {
    // Warning: this will consume the data from the reply.
    // It's usually better to readAll() in the slot and pass it as responseData.
    data = reply->readAll();
  }

  QJsonDocument doc = QJsonDocument::fromJson(data);
  QJsonObject errorObj;
  if (doc.isObject()) {
    apiError.setRawResponse(doc.object());
    if (doc.object().contains(QStringLiteral("error"))) {
      QJsonValue errVal = doc.object().value(QStringLiteral("error"));
      if (errVal.isObject()) {
        errorObj = errVal.toObject();
      } else if (errVal.isString()) {
        errorObj[QStringLiteral("message")] = errVal.toString();
      }
    } else if (doc.object().contains(QStringLiteral("message"))) {
      errorObj[QStringLiteral("message")] = doc.object().value(QStringLiteral("message")).toString();
    }
  }

  QString status = errorObj.value(QStringLiteral("status")).toString();
  QString apiMessage = errorObj.value(QStringLiteral("message")).toString();

  if (reply->error() == QNetworkReply::OperationCanceledError) {
    apiError.setType(ApiError::Type::Canceled);
    apiError.setMessage(QStringLiteral("Operation canceled."));
    return apiError;
  }

  if (statusCode == 401 || statusCode == 403) {
    apiError.setType(ApiError::Type::Authentication);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Authentication failed.") : apiMessage);
  } else if (statusCode == 429 || status == QStringLiteral("RESOURCE_EXHAUSTED")) {
    apiError.setType(ApiError::Type::RateLimit);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Rate limit exceeded.") : apiMessage);
  } else if (statusCode == 404 || status == QStringLiteral("NOT_FOUND")) {
    apiError.setType(ApiError::Type::NotFound);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Resource not found.") : apiMessage);
  } else if (status == QStringLiteral("FAILED_PRECONDITION")) {
    apiError.setType(ApiError::Type::PreconditionFailed);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Precondition failed.") : apiMessage);
  } else if (statusCode == 400 || status == QStringLiteral("INVALID_ARGUMENT")) {
    apiError.setType(ApiError::Type::Validation);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Validation error.") : apiMessage);

    if (errorObj.contains(QStringLiteral("details"))) {
      QJsonArray detailsArray = errorObj.value(QStringLiteral("details")).toArray();
      QStringList detailsList;
      for (const QJsonValue &detail : detailsArray) {
        if (detail.isObject()) {
          QJsonObject detailObj = detail.toObject();
          if (detailObj.value(QStringLiteral("@type")).toString() ==
              QStringLiteral("type.googleapis.com/google.rpc.BadRequest")) {
            QJsonArray fieldViolations = detailObj.value(QStringLiteral("fieldViolations")).toArray();
            for (const QJsonValue &violation : fieldViolations) {
              if (violation.isObject()) {
                QJsonObject violationObj = violation.toObject();
                detailsList.append(QStringLiteral("%1: %2")
                                       .arg(violationObj.value(QStringLiteral("field")).toString())
                                       .arg(violationObj.value(QStringLiteral("description")).toString()));
              }
            }
          } else if (detailObj.contains(QStringLiteral("description"))) {
            detailsList.append(detailObj.value(QStringLiteral("description")).toString());
          } else if (detailObj.contains(QStringLiteral("message"))) {
            detailsList.append(detailObj.value(QStringLiteral("message")).toString());
          }
        }
      }
      if (!detailsList.isEmpty()) {
        apiError.setDetails(detailsList.join(QStringLiteral("\n")));
      }
    }
  } else if (statusCode >= 500) {
    apiError.setType(ApiError::Type::ServerError);
    apiError.setMessage(apiMessage.isEmpty() ? QStringLiteral("Server error.") : apiMessage);
  } else if (reply->error() != QNetworkReply::NoError) {
    apiError.setType(ApiError::Type::Network);
    apiError.setMessage(reply->errorString());
  } else {
    apiError.setType(ApiError::Type::Unknown);
    apiError.setMessage(apiMessage.isEmpty() ? reply->errorString() : apiMessage);
  }

  if (apiError.message().isEmpty()) {
    apiError.setMessage(reply->errorString());
  }

  return apiError;
}
