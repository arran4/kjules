#ifndef JOBDATA_H
#define JOBDATA_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

struct JobAttemptData {
  QString id;
  QString julesSessionId;
  QJsonObject requestSnapshot;
  QString dispatchState;
  QString julesState;
  QDateTime createdAt;
  QDateTime updatedAt;
  QJsonArray launchErrors;
  QJsonObject rawResponse;
  QJsonObject prMetadata;
  bool isAccepted = false;

  QJsonObject toJson() const;
  static JobAttemptData fromJson(const QJsonObject &obj);
};

struct JobData {
  QString id;
  QJsonObject canonicalRequest;
  QString source;
  QString startingBranch;
  QString prompt;
  QString automationMode;
  bool planApproval = false;
  bool ignoreConcurrency = false;
  int priority = 0;
  QDateTime createdAt;
  QDateTime updatedAt;
  QJsonObject lifecycleMetadata;
  QVector<JobAttemptData> attempts;
  QString acceptedAttemptId;
  QJsonObject legacyMetadata;

  QJsonObject toJson() const;
  static JobData fromJson(const QJsonObject &obj);
};

#endif // JOBDATA_H
