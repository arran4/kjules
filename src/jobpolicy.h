#ifndef JOBPOLICY_H
#define JOBPOLICY_H

#include "jobdata.h"

class JobPolicy {
public:
  static bool isAttemptTerminal(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("COMPLETED") || attempt.julesState == QLatin1String("ERROR") ||
           attempt.julesState == QLatin1String("ERROR_STATE") || attempt.julesState == QLatin1String("FAILED") ||
           attempt.julesState == QLatin1String("CANCELED") || attempt.dispatchState == QLatin1String("FAILED") ||
           attempt.dispatchState == QLatin1String("ERROR") || attempt.dispatchState == QLatin1String("CANCELED");
  }

  static bool isAttemptSuccessful(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("COMPLETED");
  }

  static bool isAttemptFailed(const JobAttemptData &attempt) {
    return attempt.julesState == QLatin1String("ERROR") || attempt.julesState == QLatin1String("ERROR_STATE") ||
           attempt.julesState == QLatin1String("FAILED") || attempt.dispatchState == QLatin1String("FAILED") ||
           attempt.dispatchState == QLatin1String("ERROR");
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

  static bool isEligibleWinner(const JobData &job, const QString &id) {
    for (const auto &attempt : job.attempts)
      if (attempt.id == id)
        return isAttemptSuccessful(attempt);
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

  enum class JobAggregateState {
    Archived,
    Pending,
    WinnerSatisfied,
    WinnerWithActive,
    AwaitingUserAction,
    Active,
    ActiveWithFailed,
    CompletedWithoutWinner,
    NeedsAttention
  };

  static QString aggregateStateToString(JobAggregateState state) {
    switch (state) {
    case JobAggregateState::Archived:
      return QStringLiteral("archived");
    case JobAggregateState::Pending:
      return QStringLiteral("pending");
    case JobAggregateState::WinnerSatisfied:
      return QStringLiteral("winner/satisfied");
    case JobAggregateState::WinnerWithActive:
      return QStringLiteral("winner+active");
    case JobAggregateState::AwaitingUserAction:
      return QStringLiteral("awaiting-user");
    case JobAggregateState::Active:
      return QStringLiteral("active");
    case JobAggregateState::ActiveWithFailed:
      return QStringLiteral("active+failed");
    case JobAggregateState::CompletedWithoutWinner:
      return QStringLiteral("completed without winner");
    case JobAggregateState::NeedsAttention:
      return QStringLiteral("needs-attention");
    }
    return QStringLiteral("unknown");
  }

  static JobAggregateState aggregateState(const JobData &job) {
    bool isArchived = (job.lifecycleMetadata.value(QStringLiteral("state")).toString() == QStringLiteral("archived") ||
                       job.legacyMetadata.value(QStringLiteral("_isArchive")).toString() == QStringLiteral("true") ||
                       job.legacyMetadata.value(QStringLiteral("_isArchive")).toBool());
    if (isArchived) {
      return JobAggregateState::Archived;
    }

    bool hasWinner = isEligibleWinner(job, job.acceptedAttemptId);

    bool hasActive = false;
    bool hasFailed = false;
    bool hasCompleted = false;
    bool hasAwaitingUser = false;

    for (const auto &attempt : job.attempts) {
      bool isTerminal = isAttemptTerminal(attempt);
      bool isFailedAttempt = isAttemptFailed(attempt);
      bool isSuccessAttempt = isAttemptSuccessful(attempt);

      if (attempt.julesState == QLatin1String("AWAITING_USER_FEEDBACK") ||
          attempt.julesState == QLatin1String("AWAITING_USER_INPUT") ||
          attempt.julesState == QLatin1String("AWAITING_PLAN_APPROVAL")) {
        hasAwaitingUser = true;
      }

      if (isFailedAttempt) {
        hasFailed = true;
      } else if (isSuccessAttempt) {
        hasCompleted = true;
      }

      if (!isTerminal && (!attempt.julesState.isEmpty() || !attempt.dispatchState.isEmpty())) {
        hasActive = true;
      }
    }

    if (hasWinner) {
      if (hasActive) {
        return JobAggregateState::WinnerWithActive;
      }
      return JobAggregateState::WinnerSatisfied;
    }

    if (hasAwaitingUser) {
      return JobAggregateState::AwaitingUserAction;
    }

    if (hasActive) {
      if (hasFailed) {
        return JobAggregateState::ActiveWithFailed;
      }
      return JobAggregateState::Active;
    }

    if (job.attempts.isEmpty()) {
      return JobAggregateState::Pending;
    }

    if (hasCompleted) {
      return JobAggregateState::CompletedWithoutWinner;
    }

    return JobAggregateState::NeedsAttention;
  }
};

#endif
