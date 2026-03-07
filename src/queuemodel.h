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
  QueueItem dequeue();
  QueueItem peek() const;
  void requeueFailed(const QueueItem &item, const QString &errorMsg,
                     const QString &rawResponse = QString());
  void removeItem(int index);
  bool isEmpty() const;
  void clear();
  int size() const;

  QueueItem getItem(int index) const;

private:
  QVector<QueueItem> m_items;
  void load();
  void save();
};

#endif // QUEUEMODEL_H
