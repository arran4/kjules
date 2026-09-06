#include "jobstore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

JobStore::JobStore(const QString &filename) : m_filename(filename) {}

bool JobStore::load() {
  QFile file(m_filename);
  if (!file.exists())
    return true; // Empty store is fine
  if (!file.open(QIODevice::ReadOnly))
    return false;

  QByteArray data = file.readAll();
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    return false;

  QJsonObject root = doc.object();

  if (!root.contains(QStringLiteral("schemaVersion")))
    return false;
  QJsonValue versionVal = root.value(QStringLiteral("schemaVersion"));
  if (!versionVal.isDouble() || versionVal.toInt() != 1)
    return false;

  if (!root.contains(QStringLiteral("jobs")))
    return false;
  QJsonValue jobsVal = root.value(QStringLiteral("jobs"));
  if (!jobsVal.isArray())
    return false;

  QJsonArray jobsArray = jobsVal.toArray();
  QVector<JobData> tempJobs;
  QSet<QString> jobIds;

  for (const QJsonValue &jobVal : jobsArray) {
    if (!jobVal.isObject())
      return false;
    QJsonObject jobObj = jobVal.toObject();

    if (!jobObj.contains(QStringLiteral("id")) || !jobObj[QStringLiteral("id")].isString())
      return false;
    QString jobId = jobObj[QStringLiteral("id")].toString();
    if (jobId.isEmpty())
      return false;
    if (jobIds.contains(jobId))
      return false;
    jobIds.insert(jobId);

    if (jobObj.contains(QStringLiteral("attempts"))) {
      if (!jobObj[QStringLiteral("attempts")].isArray())
        return false;
      QJsonArray attemptArray = jobObj[QStringLiteral("attempts")].toArray();
      QSet<QString> attemptIds;
      for (const QJsonValue &attVal : attemptArray) {
        if (!attVal.isObject())
          return false;
        QJsonObject attObj = attVal.toObject();
        if (!attObj.contains(QStringLiteral("id")) || !attObj[QStringLiteral("id")].isString())
          return false;
        QString attId = attObj[QStringLiteral("id")].toString();
        if (attId.isEmpty())
          return false;
        if (attemptIds.contains(attId))
          return false;
        attemptIds.insert(attId);
      }
      if (jobObj.contains(QStringLiteral("acceptedAttemptId"))) {
        if (!jobObj[QStringLiteral("acceptedAttemptId")].isString())
          return false;
        QString accId = jobObj[QStringLiteral("acceptedAttemptId")].toString();
        if (!accId.isEmpty() && !attemptIds.contains(accId))
          return false;
      }
    }

    tempJobs.append(JobData::fromJson(jobObj));
  }

  m_jobs = tempJobs;
  return true;
}

bool JobStore::save() const {
  QFileInfo fi(m_filename);
  QDir dir = fi.dir();
  if (!dir.exists()) {
    if (!dir.mkpath(QStringLiteral(".")))
      return false;
  }

  QSaveFile saveFile(m_filename);
  saveFile.setDirectWriteFallback(false);
  if (!saveFile.open(QIODevice::WriteOnly))
    return false;

  QJsonArray jobsArray;
  for (const auto &job : m_jobs) {
    jobsArray.append(job.toJson());
  }

  QJsonObject root;
  root[QStringLiteral("schemaVersion")] = 1;
  root[QStringLiteral("jobs")] = jobsArray;

  QJsonDocument doc(root);
  if (saveFile.write(doc.toJson(QJsonDocument::Compact)) == -1)
    return false;

  return saveFile.commit();
}
