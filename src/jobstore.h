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
  JobData *getJobById(const QString &id);
  JobData *getJobByAttemptId(const QString &attemptId);
  JobData *getJobBySessionId(const QString &sessionId);
  void updateJob(const JobData &job);
  void removeJob(const QString &id);

  // For migration seams
  static JobStore fromMemory(const QVector<JobData> &jobs);

private:
  QString m_filename;
  QVector<JobData> m_jobs;
  int m_schemaVersion = 1;
};

#endif // JOBSTORE_H
