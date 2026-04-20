#ifndef BLOCKEDTREEMODEL_H
#define BLOCKEDTREEMODEL_H

#include <QAbstractItemModel>
#include <QPointer>

class SourceModel;
class QueueModel;

class BlockedTreeModel : public QAbstractItemModel {
  Q_OBJECT

public:
  explicit BlockedTreeModel(SourceModel *sourceModel, QueueModel *queueModel,
                            QObject *parent = nullptr);

  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  int blockedSourcesCount() const;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  enum Roles { IsSourceRole = Qt::UserRole + 1, SourceIdRole, QueueIndexRole };

private Q_SLOTS:
  void onSourceModelChanged();
  void onQueueModelChanged();

private:
  void rebuildTree();

  struct Node {
    bool isSource;
    QString sourceId;
    int queueIndex;
    QString display;
    QVector<Node *> children;
    Node *parent;

    ~Node() { qDeleteAll(children); }
  };

  Node *m_rootNode;
  QPointer<SourceModel> m_sourceModel;
  QPointer<QueueModel> m_queueModel;
};

#endif // BLOCKEDTREEMODEL_H
