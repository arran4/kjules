#ifndef JOBPOLICY_H
#define JOBPOLICY_H

#include "jobdata.h"

class JobPolicy {
public:
    static bool consumesConcurrency(const JobAttemptData& attempt) {
        if (attempt.julesState == QStringLiteral("ERROR") || attempt.julesState == QStringLiteral("CANCELED")) {
            return false;
        }
        if (attempt.dispatchState == QStringLiteral("FAILED")) {
            return false;
        }
        return true;
    }

    static bool hasViableActiveAttempt(const JobData& job) {
        for (const auto& attempt : job.attempts) {
            if (consumesConcurrency(attempt)) {
                return true;
            }
        }
        return false;
    }

    static bool needsAttention(const JobData& job) {
        if (job.attempts.isEmpty()) return false;

        bool allFailed = true;
        for (const auto& attempt : job.attempts) {
            if (consumesConcurrency(attempt) || attempt.julesState == QStringLiteral("COMPLETED")) {
                allFailed = false;
                break;
            }
        }
        return allFailed;
    }

    static bool isSuccessfullyComplete(const JobData& job) {
        if (!job.acceptedAttemptId.isEmpty()) return true;
        for (const auto& attempt : job.attempts) {
            if (attempt.julesState == QStringLiteral("COMPLETED")) {
                return true;
            }
        }
        return false;
    }

    static bool shouldRemainInFollowing(const JobData& job) {
        return hasViableActiveAttempt(job) || needsAttention(job);
    }

    static bool hasOutstandingAttempts(const JobData& job) {
        for (const auto& attempt : job.attempts) {
            if (consumesConcurrency(attempt) && attempt.julesState != QStringLiteral("COMPLETED")) {
                return true;
            }
        }
        return false;
    }
};

#endif // JOBPOLICY_H
