#ifndef QUEUEMODEL_H
#define QUEUEMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QVector>

struct QueueItem {
  QJsonObject requestData; // contains source, prompt, automationMode
  int errorCount = 0;
  QString lastError;
  QString lastResponse;
  QDateTime lastTry;

  bool isWaitItem = false;
  bool isDailyLimitWait = false;
  int waitSeconds = 0;
  QDateTime waitStartTime;

  QJsonObject toJson() const;
  static QueueItem fromJson(const QJsonObject &obj);
};

class QueueModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    RequestDataRole = Qt::UserRole + 1,
    ErrorCountRole,
    LastErrorRole,
    LastTryRole,
    SummaryRole,
    StatusRole
  };

  explicit QueueModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void enqueue(const QJsonObject &requestData);
  void updateItem(int index, const QueueItem &item);
  QueueItem dequeue();
  QueueItem peek() const;
  void requeueFailed(const QueueItem &item, const QString &errorMsg,
                     const QString &rawResponse = QString());
  void requeueTransient(const QueueItem &item);
  void prependWaitItem(const QueueItem &item);
  void recordRun();
  void checkAndPrependDailyLimitWait();
  void removeItem(int index);
  bool isEmpty() const;
  void clear();
  int size() const;

  QueueItem getItem(int index) const;

private:
  QVector<QueueItem> m_items;
  QVector<QDateTime> m_runTimestamps;
  void pruneRunTimestamps();
  int m_jobsSinceLastWait = 0;
  void load();
  void save();
};

#endif // QUEUEMODEL_H
