#ifndef LEGACYDATAREPAIR_H
#define LEGACYDATAREPAIR_H

#include <QJsonArray>
#include <QString>

class SessionModel;
class QueueModel;

struct LegacyDataRepairResult {
  int legacyFollowingCount = 0;
  int currentFollowingCount = 0;
  int followingToRecover = 0;
  int followingAlreadyPresent = 0;

  int legacyQueueCount = 0;
  int currentQueueCount = 0;
  int queueToRecover = 0;
  int queueAlreadyPresent = 0;

  int followingSkippedInvalid = 0;
  int queueSkippedInvalid = 0;

  QString error;
  QString fatalError;
};

class LegacyDataRepair {
public:
  LegacyDataRepairResult analyze(SessionModel *sessionModel, QueueModel *queueModel);
  LegacyDataRepairResult performMerge(SessionModel *sessionModel, QueueModel *queueModel);

private:
  QString getLegacyDataPath() const;
  QString getCurrentDataPath() const;

  QJsonArray parseLegacySessions(const QString &path, bool &outIsMalformed) const;
  QJsonArray parseLegacyQueue(const QString &path, bool &outIsMalformed) const;

  bool backupCurrentFiles(const QString &currentDataPath, QString &errorOut) const;

  QJsonArray m_cachedSessionsToRecover;
  QJsonArray m_cachedQueueToRecover;
};

#endif // LEGACYDATAREPAIR_H
