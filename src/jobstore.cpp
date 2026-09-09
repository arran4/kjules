#include "jobstore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

JobStore::JobStore(const QString &filename) {
  QFileInfo info(filename);
  if (info.isAbsolute()) {
    m_filename = filename;
  } else {
    m_filename = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/") + filename;
  }
}

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

    if (jobObj.contains(QStringLiteral("createdAt")) && !jobObj[QStringLiteral("createdAt")].isString())
      return false;
    if (jobObj.contains(QStringLiteral("updatedAt")) && !jobObj[QStringLiteral("updatedAt")].isString())
      return false;
    if (jobObj.contains(QStringLiteral("priority")) && !jobObj[QStringLiteral("priority")].isDouble())
      return false;
    if (jobObj.contains(QStringLiteral("planApproval")) && !jobObj[QStringLiteral("planApproval")].isBool())
      return false;
    if (jobObj.contains(QStringLiteral("ignoreConcurrency")) && !jobObj[QStringLiteral("ignoreConcurrency")].isBool())
      return false;
    if (jobObj.contains(QStringLiteral("canonicalRequest")) && !jobObj[QStringLiteral("canonicalRequest")].isObject())
      return false;

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

        if (attObj.contains(QStringLiteral("createdAt")) && !attObj[QStringLiteral("createdAt")].isString())
          return false;
        if (attObj.contains(QStringLiteral("updatedAt")) && !attObj[QStringLiteral("updatedAt")].isString())
          return false;
        if (attObj.contains(QStringLiteral("requestSnapshot")) && !attObj[QStringLiteral("requestSnapshot")].isObject())
          return false;
        if (attObj.contains(QStringLiteral("launchErrors")) && !attObj[QStringLiteral("launchErrors")].isArray())
          return false;
        if (attObj.contains(QStringLiteral("julesSessionId")) && !attObj[QStringLiteral("julesSessionId")].isString())
          return false;
      }
      if (jobObj.contains(QStringLiteral("acceptedAttemptId"))) {
        if (!jobObj[QStringLiteral("acceptedAttemptId")].isString())
          return false;
        QString accId = jobObj[QStringLiteral("acceptedAttemptId")].toString();
        if (!accId.isEmpty() && !attemptIds.contains(accId))
          return false;
      }
    } else {
      if (jobObj.contains(QStringLiteral("acceptedAttemptId"))) {
        if (!jobObj[QStringLiteral("acceptedAttemptId")].isString())
          return false;
        if (!jobObj[QStringLiteral("acceptedAttemptId")].toString().isEmpty())
          return false;
      }
    }

    JobData parsed = JobData::fromJson(jobObj);
    if (!parsed.createdAt.isValid() && jobObj.contains(QStringLiteral("createdAt")))
      return false;
    if (!parsed.updatedAt.isValid() && jobObj.contains(QStringLiteral("updatedAt")))
      return false;

    for (const auto &a : parsed.attempts) {
      if (!a.createdAt.isValid() && !a.createdAt.isNull())
        return false;
      if (!a.updatedAt.isValid() && !a.updatedAt.isNull())
        return false;
    }

    tempJobs.append(parsed);
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

JobStore JobStore::fromMemory(const QVector<JobData> &jobs) {
  JobStore store;
  store.setJobs(jobs);
  return store;
}

JobData *JobStore::getJobById(const QString &id) {
  for (int i = 0; i < m_jobs.size(); ++i) {
    if (m_jobs[i].id == id) {
      return &m_jobs[i];
    }
  }
  return nullptr;
}

JobData *JobStore::getJobByAttemptId(const QString &attemptId) {
  for (int i = 0; i < m_jobs.size(); ++i) {
    for (const JobAttemptData &attempt : m_jobs[i].attempts) {
      if (attempt.id == attemptId) {
        return &m_jobs[i];
      }
    }
  }
  return nullptr;
}

JobData *JobStore::getJobBySessionId(const QString &sessionId) {
  for (int i = 0; i < m_jobs.size(); ++i) {
    for (const JobAttemptData &attempt : m_jobs[i].attempts) {
      if (attempt.julesSessionId == sessionId) {
        return &m_jobs[i];
      }
    }
  }
  return nullptr;
}

void JobStore::updateJob(const JobData &job) {
  for (int i = 0; i < m_jobs.size(); ++i) {
    if (m_jobs[i].id == job.id) {
      m_jobs[i] = job;
      return;
    }
  }
}

void JobStore::removeJob(const QString &id) {
  for (int i = 0; i < m_jobs.size(); ++i) {
    if (m_jobs[i].id == id) {
      m_jobs.removeAt(i);
      return;
    }
  }
}
