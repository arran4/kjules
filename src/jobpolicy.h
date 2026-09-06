#ifndef JOBPOLICY_H
#define JOBPOLICY_H

#include "jobdata.h"

class JobPolicy {
public:
  static bool isAttemptTerminal(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("COMPLETED") || attempt.julesState == QLatin1String("ERROR") ||
           attempt.julesState == QLatin1String("CANCELED") || attempt.dispatchState == QLatin1String("FAILED") ||
           attempt.dispatchState == QLatin1String("CANCELED");
  }

  static bool isAttemptSuccessful(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("COMPLETED");
  }

  static bool isAttemptFailed(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("ERROR") || attempt.dispatchState == QLatin1String("FAILED");
  }

  static bool consumesConcurrency(const JobAttemptData &attempt) {
    if (isAttemptTerminal(attempt)) {
      return false;
    }
    if (attempt.julesState.isEmpty() && attempt.dispatchState.isEmpty()) {
      return false;
    }
    return true;
  }

  static bool isValidAttempt(const JobData &job, const QString &attemptId) {
    for (const auto &a : job.attempts) {
      if (a.id == attemptId)
        return true;
    }
    return false;
  }

  static bool hasViableActiveAttempt(const JobData &job) {
    for (const auto &attempt : job.attempts) {
      if (!isAttemptTerminal(attempt) && (!attempt.julesState.isEmpty() || !attempt.dispatchState.isEmpty()))
        return true;
    }
    return false;
  }

  static bool hasError(const JobData &job) {
    for (const auto &attempt : job.attempts) {
      if (isAttemptFailed(attempt))
        return true;
    }
    return false;
  }

  static bool isSuccessfullyComplete(const JobData &job) {
    if (!job.acceptedAttemptId.isEmpty() && isValidAttempt(job, job.acceptedAttemptId)) {
      for (const auto &attempt : job.attempts) {
        if (attempt.id == job.acceptedAttemptId && isAttemptSuccessful(attempt)) {
          return true;
        }
      }
    }
    for (const auto &attempt : job.attempts) {
      if (isAttemptSuccessful(attempt))
        return true;
    }
    return false;
  }

  static bool needsAttention(const JobData &job) {
    if (job.attempts.isEmpty())
      return false;
    if (isSuccessfullyComplete(job))
      return false;
    if (hasViableActiveAttempt(job))
      return false;

    return hasError(job);
  }

  static bool shouldRemainInFollowing(const JobData &job) { return hasViableActiveAttempt(job) || needsAttention(job); }

  static bool hasOutstandingAttempts(const JobData &job) { return hasViableActiveAttempt(job); }
};

#endif
