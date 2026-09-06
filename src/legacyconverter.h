#ifndef LEGACYCONVERTER_H
#define LEGACYCONVERTER_H

#include "jobdata.h"
#include "queuemodel.h"
#include "sessionmodel.h"
#include "errorsmodel.h"
#include <QVector>

class LegacyConverter {
public:
    static QVector<JobData> convertQueue(const QVector<QueueItem>& items, bool isHolding);
    static QVector<JobData> convertSessions(const QJsonArray& sessions, bool isArchive);
    static QVector<JobData> convertErrors(const QJsonArray& errors);

    static JobData fromQueueItem(const QueueItem& item, bool isHolding);
    static JobData fromSession(const QJsonObject& session, bool isArchive);
    static JobData fromError(const QJsonObject& error);
};

#endif // LEGACYCONVERTER_H
