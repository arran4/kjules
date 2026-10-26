#ifndef FOLLOWINGREFRESHEVALUATOR_H
#define FOLLOWINGREFRESHEVALUATOR_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <optional>

class FollowingRefreshEvaluator {
public:
  static bool isPrMerged(const QJsonObject &prObj) {
    if (prObj.isEmpty()) {
      return false;
    }
    if (prObj.value(QStringLiteral("merged")).toBool()) {
      return true;
    }
    QString state = prObj.value(QStringLiteral("state")).toString();
    if (state.isEmpty()) {
      state = prObj.value(QStringLiteral("status")).toString();
    }
    if (state.compare(QStringLiteral("merged"), Qt::CaseInsensitive) == 0) {
      return true;
    }
    QJsonValue mergedAtVal = prObj.value(QStringLiteral("merged_at"));
    if (mergedAtVal.isString()) {
      QString mergedAt = mergedAtVal.toString().trimmed();
      if (!mergedAt.isEmpty()) {
        return true;
      }
    }
    return false;
  }

  static QString extractPrStatus(const QJsonObject &prObj) {
    if (prObj.isEmpty()) {
      return QString();
    }
    if (isPrMerged(prObj)) {
      return QStringLiteral("merged");
    }
    QString state = prObj.value(QStringLiteral("state")).toString();
    if (state.isEmpty()) {
      state = prObj.value(QStringLiteral("status")).toString();
    }
    return state;
  }

  static QString extractAttemptPrStatus(const QJsonObject &rawResponse, const QJsonObject &prMetadata) {
    if (rawResponse.contains(QStringLiteral("githubPrInfo"))) {
      return extractPrStatus(rawResponse.value(QStringLiteral("githubPrInfo")).toObject());
    }
    if (!prMetadata.isEmpty()) {
      return extractPrStatus(prMetadata);
    }
    return QString();
  }

  static bool isAttemptEligible(const QString &julesState, const QJsonObject &rawResponse,
                                const QJsonObject &prMetadata) {
    QString prStatus = extractAttemptPrStatus(rawResponse, prMetadata);
    return isSessionEligible(julesState, prStatus);
  }

  static bool isSessionEligible(const QString &state, const QString &githubPrStatus) {
    if (state == QStringLiteral("CANCELED") || state == QStringLiteral("ERROR")) {
      return false;
    }

    if (state == QStringLiteral("COMPLETED")) {
      return githubPrStatus.compare(QStringLiteral("merged"), Qt::CaseInsensitive) != 0 &&
             githubPrStatus.compare(QStringLiteral("closed"), Qt::CaseInsensitive) != 0;
    }

    return true;
  }

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
