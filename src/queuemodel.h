#ifndef QUEUEMODEL_H
#define QUEUEMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

struct QueueItem {
  QJsonObject requestData; // contains source, prompt, automationMode
  int errorCount = 0;
  QString lastError;
  QString lastResponse;
  QDateTime lastTry;
  QJsonArray pastErrors; // stores history of errors

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

  explicit QueueModel(QObject *parent = nullptr,
                      const QString &filename = QStringLiteral("queue.json"),
                      bool isHolding = false);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Qt::ItemFlags flags(const QModelIndex &index) const override;
  Qt::DropActions supportedDropActions() const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;
  bool removeRows(int row, int count,
                  const QModelIndex &parent = QModelIndex()) override;

  void enqueue(const QJsonObject &requestData);
  void enqueueItem(const QueueItem &item);
  void insertItem(int index, const QueueItem &item);
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
  void refreshWaitItems();

  QueueItem getItem(int index) const;
  void moveItem(int from, int to);

private:
  QVector<QueueItem> m_items;
  QVector<QDateTime> m_runTimestamps;
  void pruneRunTimestamps();
  int m_jobsSinceLastWait = 0;
  QString m_filename;
  bool m_isHolding;
  void load();
  void save();
  void removeTrailingWaitItems();
};

#endif // QUEUEMODEL_H
