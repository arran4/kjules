#include "blockedtreemodel.h"
#include "queuemodel.h"
#include "sourcemodel.h"
#include <KLocalizedString>

BlockedTreeModel::BlockedTreeModel(SourceModel *sourceModel,
                                   QueueModel *queueModel, QObject *parent)
    : QAbstractItemModel(parent), m_sourceModel(sourceModel),
      m_queueModel(queueModel),
      m_rootNode(new Node{false, "", -1, "", {}, nullptr}) {

  connect(m_sourceModel, &QAbstractTableModel::dataChanged, this,
          &BlockedTreeModel::onSourceModelChanged);
  connect(m_sourceModel, &QAbstractTableModel::modelReset, this,
          &BlockedTreeModel::onSourceModelChanged);
  connect(m_sourceModel, &QAbstractTableModel::rowsInserted, this,
          &BlockedTreeModel::onSourceModelChanged);
  connect(m_sourceModel, &QAbstractTableModel::rowsRemoved, this,
          &BlockedTreeModel::onSourceModelChanged);

  connect(m_queueModel, &QAbstractListModel::dataChanged, this,
          &BlockedTreeModel::onQueueModelChanged);
  connect(m_queueModel, &QAbstractListModel::modelReset, this,
          &BlockedTreeModel::onQueueModelChanged);
  connect(m_queueModel, &QAbstractListModel::rowsInserted, this,
          &BlockedTreeModel::onQueueModelChanged);
  connect(m_queueModel, &QAbstractListModel::rowsRemoved, this,
          &BlockedTreeModel::onQueueModelChanged);

  rebuildTree();
}

void BlockedTreeModel::onSourceModelChanged() { rebuildTree(); }
void BlockedTreeModel::onQueueModelChanged() { rebuildTree(); }

void BlockedTreeModel::rebuildTree() {
  Q_EMIT layoutAboutToBeChanged();
  beginResetModel();
  qDeleteAll(m_rootNode->children);
  m_rootNode->children.clear();

  if (!m_sourceModel || !m_queueModel) {
    endResetModel();
    Q_EMIT layoutChanged();
    return;
  }

  QHash<QString, Node *> sourceNodes;

  for (int i = 0; i < m_sourceModel->rowCount(); ++i) {
    QModelIndex idx = m_sourceModel->index(i, 0);
    QString id = m_sourceModel->data(idx, SourceModel::IdRole).toString();
    QString name = m_sourceModel->data(idx, SourceModel::NameRole).toString();

    Node *node = new Node{true, id, -1, name, {}, m_rootNode};
    sourceNodes.insert(id, node);
    m_rootNode->children.append(node);
  }

  for (int i = 0; i < m_queueModel->rowCount(); ++i) {
    QueueItem item = m_queueModel->getItem(i);
    if (!item.isBlocked) {
      continue;
    }

    QString source = item.requestData.value(QStringLiteral("sourceContext"))
                         .toObject()
                         .value(QStringLiteral("source"))
                         .toString();
    if (source.isEmpty()) {
      source = item.requestData.value(QStringLiteral("source")).toString();
    }

    if (!sourceNodes.contains(source)) {
      Node *node = new Node{true, source, -1, source, {}, m_rootNode};
      sourceNodes.insert(source, node);
      m_rootNode->children.append(node);
    }

    Node *parentNode = sourceNodes.value(source);
    QString prompt =
        item.requestData.value(QStringLiteral("prompt")).toString();
    if (prompt.length() > 50) {
      prompt = prompt.left(50) + "...";
    }
    QString display = QStringLiteral("Blocked: ") + prompt;
    Node *childNode = new Node{false, source, i, display, {}, parentNode};
    parentNode->children.append(childNode);
  }

  endResetModel();
  Q_EMIT layoutChanged();
}

QModelIndex BlockedTreeModel::index(int row, int column,
                                    const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  Node *parentNode;
  if (!parent.isValid()) {
    parentNode = m_rootNode;
  } else {
    parentNode = static_cast<Node *>(parent.internalPointer());
  }

  if (row < parentNode->children.size()) {
    return createIndex(row, column, parentNode->children.at(row));
  }
  return QModelIndex();
}

QModelIndex BlockedTreeModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) {
    return QModelIndex();
  }

  Node *childNode = static_cast<Node *>(index.internalPointer());
  Node *parentNode = childNode->parent;

  if (parentNode == m_rootNode || parentNode == nullptr) {
    return QModelIndex();
  }

  int row = parentNode->parent->children.indexOf(parentNode);
  return createIndex(row, 0, parentNode);
}

int BlockedTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) {
    return 0;
  }

  Node *parentNode;
  if (!parent.isValid()) {
    parentNode = m_rootNode;
  } else {
    parentNode = static_cast<Node *>(parent.internalPointer());
  }

  return parentNode->children.size();
}

int BlockedTreeModel::blockedSourcesCount() const {
  int count = 0;
  for (Node *node : m_rootNode->children) {
    if (!node->children.isEmpty()) {
      count++;
    }
  }
  return count;
}

int BlockedTreeModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 1;
}

QVariant BlockedTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  Node *node = static_cast<Node *>(index.internalPointer());

  if (role == Qt::DisplayRole) {
    if (node->isSource) {
      return QStringLiteral("%1 (%2)")
          .arg(node->display)
          .arg(node->children.size());
    }
    return node->display;
  } else if (role == IsSourceRole) {
    return node->isSource;
  } else if (role == SourceIdRole) {
    return node->sourceId;
  } else if (role == QueueIndexRole) {
    return node->queueIndex;
  }

  return QVariant();
}
