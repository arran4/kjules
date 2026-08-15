#ifndef FOLLOWINGREFRESHEVALUATOR_H
#define FOLLOWINGREFRESHEVALUATOR_H

#include <QDateTime>
#include <QJsonObject>
#include <optional>

class FollowingRefreshEvaluator {
public:
  static int effectiveIntervalSeconds(int globalIntervalSeconds,
                                      const std::optional<int> &localRefreshIntervalMinutes) {
    if (localRefreshIntervalMinutes.has_value()) {
      int localVal = localRefreshIntervalMinutes.value();
      if (localVal == 0) {
        return 0; // Disabled
      }
      if (localVal > 0) {
        return localVal * 60;
      }
      // Negative / -1 -> inherit global
      return globalIntervalSeconds;
    }
    return globalIntervalSeconds;
  }

  static bool shouldRefresh(const QDateTime &now, const QDateTime &lastRefreshedOrFallback, int effectiveIntervalSecs,
                            bool isInFlight, const QDateTime &lastFailedAt, int failureCooldownSeconds = 300) {
    if (isInFlight) {
      return false;
    }
    if (effectiveIntervalSecs <= 0) {
      return false;
    }
    if (lastFailedAt.isValid() && lastFailedAt.addSecs(failureCooldownSeconds) > now) {
      return false;
    }
    if (!lastRefreshedOrFallback.isValid()) {
      return true;
    }
    return lastRefreshedOrFallback.addSecs(effectiveIntervalSecs) <= now;
  }
};

#endif // FOLLOWINGREFRESHEVALUATOR_H
