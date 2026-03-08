#ifndef QUEUEMANAGER_H
#define QUEUEMANAGER_H

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

class APIManager;

struct QueueItem {
  enum Type { SessionJob, Wait };
  Type type;

  // For SessionJob
  QString source;
  QString prompt;
  QString automationMode;

  // For Wait
  int remainingSeconds;
};

class QueueManager : public QAbstractListModel {
  Q_OBJECT

public:
  enum QueueRoles {
    TypeRole = Qt::UserRole + 1,
    DescriptionRole,
    StatusRole,
    TimeRemainingRole
  };

  explicit QueueManager(APIManager *apiManager, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void addJobs(const QStringList &sources, const QString &prompt,
               const QString &automationMode);

  enum Tier { Free, Pro, Max };
  void setTier(Tier tier);
  Tier currentTier() const { return m_tier; }

private Q_SLOTS:
  void processNext();
  void onTick();

private:
  APIManager *m_apiManager;
  QList<QueueItem> m_queue;
  QTimer *m_timer;
  bool m_isProcessing;
  int m_jobsCompletedSinceLastWait;
  Tier m_tier;

  int jobsBeforeWait() const;
};

#endif // QUEUEMANAGER_H
