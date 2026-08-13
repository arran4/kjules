#ifndef APIERROR_H
#define APIERROR_H

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QString>

class ApiError {
public:
  enum class Type {
    Unknown,
    Network,
    Authentication,
    RateLimit,
    NotFound,
    PreconditionFailed,
    Validation,
    ServerError,
    Canceled
  };

  ApiError();
  ApiError(Type type, const QString &message, int httpStatusCode = 0);

  Type type() const;
  void setType(Type type);

  QString message() const;
  void setMessage(const QString &message);

  QString details() const;
  void setDetails(const QString &details);

  int httpStatusCode() const;
  void setHttpStatusCode(int code);

  QJsonObject rawResponse() const;
  void setRawResponse(const QJsonObject &response);

  QJsonObject toJson() const;
  static ApiError fromJson(const QJsonObject &json);

private:
  Type m_type;
  QString m_message;
  QString m_details;
  int m_httpStatusCode;
  QJsonObject m_rawResponse;
};

class ApiErrorDetector {
public:
  static ApiError detect(QNetworkReply *reply, const QByteArray &responseData = QByteArray());
};

#endif // APIERROR_H
