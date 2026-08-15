#ifndef QUEUESCHEDULER_H
#define QUEUESCHEDULER_H

#include <QDateTime>
#include <QString>

class QueueScheduler {
public:
  struct State {
    QDateTime nextProcessAt;
    QDateTime backoffUntil;
    QString backoffReason;

    bool isBackoffActive(const QDateTime &now) const { return backoffUntil.isValid() && now < backoffUntil; }

    bool isDue(const QDateTime &now) const {
      if (isBackoffActive(now)) {
        return false;
      }
      if (backoffUntil.isValid() && now >= backoffUntil) {
        return true;
      }
      return !nextProcessAt.isValid() || now >= nextProcessAt;
    }

    void advanceOnDispatch(const QDateTime &now, int intervalMinutes) {
      nextProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
      backoffUntil = QDateTime();
      backoffReason.clear();
    }

    void advanceOnCheckNoWork(const QDateTime &now, int intervalMinutes) {
      if (backoffUntil.isValid() && now >= backoffUntil) {
        backoffUntil = QDateTime();
        backoffReason.clear();
      }
      nextProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
    }

    void applyBackoff(const QDateTime &now, int backoffSeconds, const QString &reason) {
      backoffUntil = now.addSecs(backoffSeconds);
      backoffReason = reason;
    }

    void clearBackoff() {
      backoffUntil = QDateTime();
      backoffReason.clear();
    }

    void updateInterval(const QDateTime &now, int intervalMinutes) {
      nextProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
      // Active backoff is NOT modified or shortened
    }
  };
};

#endif // QUEUESCHEDULER_H
