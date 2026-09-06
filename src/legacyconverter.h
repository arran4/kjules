#ifndef LEGACYCONVERTER_H
#define LEGACYCONVERTER_H

#include "jobdata.h"
#include "queuemodel.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

struct LegacyData {
  QVector<QueueItem> queueItems;
  QVector<QueueItem> holdingItems;
  QJsonArray activeSessions;
  QJsonArray archivedSessions;
  QJsonArray errors;
};

struct ConversionResult {
  QVector<JobData> jobs;
  QJsonArray unattachedErrors;
};

class LegacyConverter {
public:
  static ConversionResult convertAll(const LegacyData &data,
                                     const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc());

  static QVector<JobData> convertQueue(const QVector<QueueItem> &items, bool isHolding,
                                       const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc());
  static QVector<JobData> convertSessions(const QJsonArray &sessions, bool isArchive,
                                          const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc());
  static QVector<JobData> convertErrors(const QJsonArray &errors,
                                        const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc());

  static JobData fromQueueItem(const QueueItem &item, bool isHolding,
                               const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc(),
                               int duplicateIndex = 0);
  static JobData fromSession(const QJsonObject &session, bool isArchive,
                             const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc(),
                             int duplicateIndex = 0);
  static JobData fromError(const QJsonObject &error,
                           const QDateTime &fallbackTimestamp = QDateTime::currentDateTimeUtc(),
                           int duplicateIndex = 0);
};

#endif
