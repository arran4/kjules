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
  if (m_simulateSaveFailure) {
    m_lastError = QStringLiteral("Simulated disk I/O failure");
    return false;
  }
  if (m_simulateSaveFailureAfterCount == 0) {
    m_lastError = QStringLiteral("Simulated disk I/O failure");
    return false;
  }
  if (m_simulateSaveFailureAfterCount > 0) {
    --m_simulateSaveFailureAfterCount;
  }

  QFileInfo fi(m_filename);
  QDir dir = fi.dir();
  if (!dir.exists()) {
    if (!dir.mkpath(dir.absolutePath())) {
      m_lastError = QStringLiteral("Failed to create directory: %1").arg(dir.absolutePath());
      qWarning() << "[JobStore::save] mkpath failed for" << dir.absolutePath();
      return false;
    }
  }

  QSaveFile saveFile(m_filename);
  saveFile.setDirectWriteFallback(false);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    m_lastError = saveFile.errorString();
    qWarning() << "[JobStore::save] open failed for" << m_filename << ":" << saveFile.errorString();
    return false;
  }

  QJsonArray jobsArray;
  for (const auto &job : m_jobs) {
    jobsArray.append(job.toJson());
  }

  QJsonObject root;
  root[QStringLiteral("schemaVersion")] = 1;
  root[QStringLiteral("jobs")] = jobsArray;

  QJsonDocument doc(root);
  if (saveFile.write(doc.toJson(QJsonDocument::Compact)) == -1) {
    m_lastError = saveFile.errorString();
    qWarning() << "[JobStore::save] write failed:" << saveFile.errorString();
    return false;
  }

  bool committed = saveFile.commit();
  if (!committed) {
    m_lastError = saveFile.errorString();
    qWarning() << "[JobStore::save] commit failed:" << saveFile.errorString();
  } else {
    m_lastError.clear();
  }
  return committed;
}

JobStore JobStore::fromMemory(const QVector<JobData> &jobs) {
  JobStore store;
  store.setJobs(jobs);
  return store;
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

bool JobStore::updateJobTransactional(const JobData &job) {
  QVector<JobData> backup = m_jobs;
  updateJob(job);
  if (save()) {
    return true;
  }
  m_jobs = backup;
  return false;
}

bool JobStore::removeJobTransactional(const QString &id) {
  QVector<JobData> backup = m_jobs;
  removeJob(id);
  if (save()) {
    return true;
  }
  m_jobs = backup;
  return false;
}

bool JobStore::addJobTransactional(const JobData &job) {
  QVector<JobData> backup = m_jobs;
  addJob(job);
  if (save()) {
    return true;
  }
  m_jobs = backup;
  return false;
}

bool JobStore::replaceJobTransactional(const QString &origJobId, const QVector<JobData> &newJobs, bool keepOrigJob) {
  QVector<JobData> backup = m_jobs;
  for (const JobData &job : newJobs) {
    addJob(job);
  }
  if (m_simulateFailureBetweenReplaceAndRemove) {
    m_lastError = QStringLiteral("Simulated failure between replacement creation and original removal");
    m_jobs = backup;
    return false;
  }
  if (!origJobId.isEmpty()) {
    JobData *origJob = getJobById(origJobId);
    if (keepOrigJob || (origJob && !origJob->attempts.isEmpty())) {
      if (origJob) {
        origJob->recordHistory(QStringLiteral("cancel_dispatch"),
                               QStringLiteral("Pending variant edited and rescheduled"));
      }
    } else {
      removeJob(origJobId);
    }
  }
  if (save()) {
    return true;
  }
  m_jobs = backup;
  return false;
}
