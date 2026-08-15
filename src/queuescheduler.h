#ifndef QUEUESCHEDULER_H
#define QUEUESCHEDULER_H

#include <QDateTime>
#include <QString>

class QueueScheduler {
public:
  bool isDue(const QDateTime &now) const {
    if (isBackoffActive(now)) {
      return false;
    }
    return !m_nextQueueProcessAt.isValid() || now >= m_nextQueueProcessAt;
  }

  bool isBackoffActive(const QDateTime &now) const {
    return m_queueBackoffUntil.isValid() && now < m_queueBackoffUntil;
  }

  QDateTime nextQueueProcessAt() const { return m_nextQueueProcessAt; }
  QDateTime queueBackoffUntil() const { return m_queueBackoffUntil; }
  QString queueBackoffReason() const { return m_queueBackoffReason; }

  void recordDispatch(const QDateTime &now, int intervalMinutes) {
    m_nextQueueProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
    clearBackoff();
  }

  void recordNoWork(const QDateTime &now, int intervalMinutes) {
    if (m_queueBackoffUntil.isValid() && now >= m_queueBackoffUntil) {
      clearBackoff();
    }
    m_nextQueueProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
  }

  void applyBackoff(const QDateTime &now, int backoffSeconds, const QString &reason) {
    m_queueBackoffUntil = now.addSecs(backoffSeconds);
    m_queueBackoffReason = reason;
    m_nextQueueProcessAt = m_queueBackoffUntil;
  }

  void clearBackoff() {
    m_queueBackoffUntil = QDateTime();
    m_queueBackoffReason.clear();
  }

  void updateInterval(const QDateTime &now, int intervalMinutes) {
    if (isBackoffActive(now)) {
      m_nextQueueProcessAt = m_queueBackoffUntil;
    } else {
      m_nextQueueProcessAt = now.addSecs(static_cast<qint64>(intervalMinutes) * 60);
    }
  }

  void setNextProcessAt(const QDateTime &dt) { m_nextQueueProcessAt = dt; }
  void setBackoffUntil(const QDateTime &dt) { m_queueBackoffUntil = dt; }
  void setBackoffReason(const QString &reason) { m_queueBackoffReason = reason; }

private:
  QDateTime m_nextQueueProcessAt;
  QDateTime m_queueBackoffUntil;
  QString m_queueBackoffReason;
};

#endif // QUEUESCHEDULER_H
