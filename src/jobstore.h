#ifndef JOBSTORE_H
#define JOBSTORE_H

#include "jobdata.h"
#include <QJsonObject>
#include <QString>
#include <QVector>

class JobStore {
public:
  explicit JobStore(const QString &filename = QStringLiteral("jobs.json"));

  bool load();
  bool save() const;

  QVector<JobData> jobs() const { return m_jobs; }
  void addJob(const JobData &job) { m_jobs.append(job); }
  void clear() { m_jobs.clear(); }
  void setJobs(const QVector<JobData> &jobs) { m_jobs = jobs; }
  JobData *getJobById(const QString &id) {
    for (int i = 0; i < m_jobs.size(); ++i) {
      if (m_jobs[i].id == id) {
        return &m_jobs[i];
      }
    }
    return nullptr;
  }
  JobData *getJobByAttemptId(const QString &attemptId) {
    for (int i = 0; i < m_jobs.size(); ++i) {
      for (JobAttemptData &attempt : m_jobs[i].attempts) {
        if (attempt.id == attemptId) {
          return &m_jobs[i];
        }
      }
    }
    return nullptr;
  }
  JobData *getJobBySessionId(const QString &sessionId) {
    for (int i = 0; i < m_jobs.size(); ++i) {
      if (m_jobs[i].id == sessionId) {
        return &m_jobs[i];
      }
      for (JobAttemptData &attempt : m_jobs[i].attempts) {
        if (attempt.julesSessionId == sessionId) {
          return &m_jobs[i];
        }
      }
    }
    return nullptr;
  }
  const JobData *getJobById(const QString &id) const {
    for (int i = 0; i < m_jobs.size(); ++i) {
      if (m_jobs[i].id == id) {
        return &m_jobs[i];
      }
    }
    return nullptr;
  }
  const JobData *getJobByAttemptId(const QString &attemptId) const {
    for (int i = 0; i < m_jobs.size(); ++i) {
      for (const JobAttemptData &attempt : m_jobs[i].attempts) {
        if (attempt.id == attemptId) {
          return &m_jobs[i];
        }
      }
    }
    return nullptr;
  }
  const JobData *getJobBySessionId(const QString &sessionId) const {
    for (int i = 0; i < m_jobs.size(); ++i) {
      if (m_jobs[i].id == sessionId) {
        return &m_jobs[i];
      }
      for (const JobAttemptData &attempt : m_jobs[i].attempts) {
        if (attempt.julesSessionId == sessionId) {
          return &m_jobs[i];
        }
      }
    }
    return nullptr;
  }
  void updateJob(const JobData &job);
  void removeJob(const QString &id);
  bool updateJobTransactional(const JobData &job);
  bool removeJobTransactional(const QString &id);
  bool addJobTransactional(const JobData &job);

  // For migration seams
  static JobStore fromMemory(const QVector<JobData> &jobs);

private:
  QString m_filename;
  QVector<JobData> m_jobs;
  int m_schemaVersion = 1;
};

#endif // JOBSTORE_H
