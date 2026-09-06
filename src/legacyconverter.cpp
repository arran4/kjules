#include "legacyconverter.h"
#include <QUuid>
#include <QJsonDocument>

namespace {
    QString canonicalizeJson(const QJsonObject& obj) {
        QJsonDocument doc(obj);
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    QString deterministicUuid(const QString& domain, const QJsonObject& legacyMetadata) {
        // Use a base UUID as the namespace for our domain
        QUuid nsUuid = QUuid::createUuidV5(QUuid(), domain);
        QString canonical = canonicalizeJson(legacyMetadata);
        return QUuid::createUuidV5(nsUuid, canonical).toString(QUuid::WithoutBraces);
    }
}

QVector<JobData> LegacyConverter::convertQueue(const QVector<QueueItem>& items, bool isHolding) {
    QVector<JobData> jobs;
    for (const auto& item : items) {
        jobs.append(fromQueueItem(item, isHolding));
    }
    return jobs;
}

QVector<JobData> LegacyConverter::convertSessions(const QJsonArray& sessions, bool isArchive) {
    QVector<JobData> jobs;
    for (const auto& sessionVal : sessions) {
        jobs.append(fromSession(sessionVal.toObject(), isArchive));
    }
    return jobs;
}

QVector<JobData> LegacyConverter::convertErrors(const QJsonArray& errors) {
    QVector<JobData> jobs;
    for (const auto& errorVal : errors) {
        jobs.append(fromError(errorVal.toObject()));
    }
    return jobs;
}

JobData LegacyConverter::fromQueueItem(const QueueItem& item, bool isHolding) {
    JobData job;
    QJsonObject legacy;
    legacy[QStringLiteral("errorCount")] = item.errorCount;
    legacy[QStringLiteral("lastError")] = item.lastError;
    legacy[QStringLiteral("lastResponse")] = item.lastResponse;
    if (item.lastTry.isValid()) {
        legacy[QStringLiteral("lastTry")] = item.lastTry.toString(Qt::ISODate);
    }
    legacy[QStringLiteral("pastErrors")] = item.pastErrors;
    job.legacyMetadata = legacy;

    job.id = deterministicUuid(QStringLiteral("queue-job-v1"), item.requestData);
    job.canonicalRequest = item.requestData;


    QJsonObject sourceCtx = item.requestData[QStringLiteral("sourceContext")].toObject();
    job.source = sourceCtx[QStringLiteral("source")].toString();
    QJsonObject githubCtx = sourceCtx[QStringLiteral("githubRepoContext")].toObject();
    job.startingBranch = githubCtx[QStringLiteral("startingBranch")].toString();

    job.prompt = item.requestData[QStringLiteral("prompt")].toString();
    job.automationMode = item.requestData[QStringLiteral("automationMode")].toString();

    QJsonObject prefs = item.requestData[QStringLiteral("preferences")].toObject();
    job.planApproval = prefs[QStringLiteral("planApproval")].toBool();
    job.ignoreConcurrency = prefs[QStringLiteral("ignoreConcurrency")].toBool();
    job.priority = prefs[QStringLiteral("priority")].toInt();

    job.createdAt = QDateTime::currentDateTimeUtc();
    job.updatedAt = QDateTime::currentDateTimeUtc();

    QJsonObject lifecycle;
    lifecycle[QStringLiteral("isHolding")] = isHolding;
    lifecycle[QStringLiteral("isBlocked")] = item.isBlocked;
    if (!item.blockMetadata.isEmpty()) {
        lifecycle[QStringLiteral("blockMetadata")] = item.blockMetadata;
    }
    job.lifecycleMetadata = lifecycle;
    return job;
}

JobData LegacyConverter::fromSession(const QJsonObject& session, bool isArchive) {
    JobData job;
    job.legacyMetadata = session; // Store all original session data
    job.id = deterministicUuid(QStringLiteral("session-job-v1"), session);

    job.source = session[QStringLiteral("source")].toString();
    job.prompt = session[QStringLiteral("prompt")].toString();
    job.createdAt = QDateTime::fromString(session[QStringLiteral("createTime")].toString(), Qt::ISODate);
    job.updatedAt = QDateTime::fromString(session[QStringLiteral("updateTime")].toString(), Qt::ISODate);

    QJsonObject lifecycle;
    lifecycle[QStringLiteral("isArchive")] = isArchive;
    if (session.contains(QStringLiteral("favouriteRank"))) lifecycle[QStringLiteral("favouriteRank")] = session[QStringLiteral("favouriteRank")].toInt();
    if (session.contains(QStringLiteral("snoozeUntil"))) lifecycle[QStringLiteral("snoozeUntil")] = session[QStringLiteral("snoozeUntil")].toString();
    if (session.contains(QStringLiteral("refreshInterval"))) lifecycle[QStringLiteral("refreshInterval")] = session[QStringLiteral("refreshInterval")].toInt();
    job.lifecycleMetadata = lifecycle;

    JobAttemptData attempt;
    attempt.id = deterministicUuid(QStringLiteral("session-attempt-v1"), session);
    attempt.julesSessionId = session[QStringLiteral("id")].toString();
    attempt.julesState = session[QStringLiteral("state")].toString();
    attempt.createdAt = job.createdAt;
    attempt.updatedAt = job.updatedAt;

    QJsonObject prMeta;
    prMeta[QStringLiteral("url")] = session[QStringLiteral("prUrl")].toString();
    prMeta[QStringLiteral("number")] = session[QStringLiteral("prNumber")].toString();
    prMeta[QStringLiteral("status")] = session[QStringLiteral("prStatus")].toString();
    prMeta[QStringLiteral("labels")] = session[QStringLiteral("prLabels")].toArray();
    attempt.prMetadata = prMeta;

    // Set canonicalRequest based on rawObject if available
    if (session.contains(QStringLiteral("rawObject"))) {
        attempt.rawResponse = session[QStringLiteral("rawObject")].toObject();
        // Extract basic data for canonicalRequest if possible, though we may lack the full original request
    }

    job.attempts.append(attempt);
    return job;
}

JobData LegacyConverter::fromError(const QJsonObject& error) {
    JobData job;
    job.legacyMetadata = error;
    job.id = deterministicUuid(QStringLiteral("error-job-v1"), error);

    job.canonicalRequest = error[QStringLiteral("request")].toObject();

    if (job.canonicalRequest.contains(QStringLiteral("sourceContext"))) {
        QJsonObject sourceCtx = job.canonicalRequest[QStringLiteral("sourceContext")].toObject();
        job.source = sourceCtx[QStringLiteral("source")].toString();
    }
    job.prompt = job.canonicalRequest[QStringLiteral("prompt")].toString();

    job.createdAt = QDateTime::fromString(error[QStringLiteral("timestamp")].toString(), Qt::ISODate);
    if (!job.createdAt.isValid()) {
        job.createdAt = QDateTime::currentDateTimeUtc();
    }
    job.updatedAt = job.createdAt;

    JobAttemptData attempt;
    attempt.id = deterministicUuid(QStringLiteral("error-attempt-v1"), error);
    attempt.requestSnapshot = job.canonicalRequest;
    attempt.dispatchState = QStringLiteral("FAILED");
    attempt.createdAt = job.createdAt;
    attempt.updatedAt = job.updatedAt;

    QJsonObject errResp;
    errResp[QStringLiteral("message")] = error[QStringLiteral("message")].toString();
    errResp[QStringLiteral("httpDetails")] = error[QStringLiteral("httpDetails")].toString();
    errResp[QStringLiteral("response")] = error[QStringLiteral("response")].toObject();

    QJsonArray errors;
    errors.append(errResp);
    attempt.launchErrors = errors;

    job.attempts.append(attempt);
    return job;
}
