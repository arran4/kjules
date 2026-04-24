#include "filtereditor.h"
#include "filterparser.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <QComboBox>
#include <QCompleter>
#include <QDateTimeEdit>

#include <QComboBox>
#include <QCompleter>
#include <QDateTimeEdit>

class FilterInputDialog : public QDialog {
public:
  QString key;
  QString value;
  QLineEdit *keyEdit;
  QWidget *valueWidget; // Can be QLineEdit, QComboBox, or QDateTimeEdit

  FilterInputDialog(const QString &promptKey, bool requireKey,
                    const QStringList &completions,
                    const QString &itemKey = QString(),
                    QWidget *parent = nullptr)
      : QDialog(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    if (requireKey) {
      layout->addWidget(new QLabel(tr("Filter Key:"), this));
      keyEdit = new QLineEdit(this);
      layout->addWidget(keyEdit);
    } else {
      keyEdit = nullptr;
    }

    layout->addWidget(new QLabel(promptKey, this));

    QString lowerKey = itemKey.toLower();
    bool isDate = lowerKey.endsWith(QStringLiteral("before")) ||
                  lowerKey.endsWith(QStringLiteral("after")) ||
                  lowerKey == QStringLiteral("createdat") ||
                  lowerKey == QStringLiteral("updatedat");

    if (isDate) {
      QDateTimeEdit *dtEdit =
          new QDateTimeEdit(QDateTime::currentDateTime(), this);
      dtEdit->setCalendarPopup(true);
      dtEdit->setDisplayFormat(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
      valueWidget = dtEdit;
    } else if (!completions.isEmpty() && !requireKey) {
      QComboBox *cb = new QComboBox(this);
      cb->setEditable(true);
      cb->addItems(completions);
      valueWidget = cb;
    } else {
      QLineEdit *le = new QLineEdit(this);
      if (!completions.isEmpty()) {
        QCompleter *completer = new QCompleter(completions, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        le->setCompleter(completer);
      }
      valueWidget = le;
    }

    layout->addWidget(valueWidget);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  }

  QString getKey() const { return keyEdit ? keyEdit->text() : QString(); }
  QString getValue() const {
    if (QDateTimeEdit *dt = qobject_cast<QDateTimeEdit *>(valueWidget)) {
      return dt->dateTime().toString(Qt::ISODate);
    } else if (QComboBox *cb = qobject_cast<QComboBox *>(valueWidget)) {
      return cb->currentText();
    } else if (QLineEdit *le = qobject_cast<QLineEdit *>(valueWidget)) {
      return le->text();
    }
    return QString();
  }
};

enum FilterItemRoles {
  NodeTypeRole = Qt::UserRole + 1,
  NodeValueRole,
  NodeKeyRole,
  NodeChildCountRole
};

enum NodeType { TypeAnd, TypeOr, TypeNot, TypeIn, TypeKV, TypeKeyword };

FilterEditor::FilterEditor(QWidget *parent)
    : QWidget(parent), m_updating(false) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  m_lineEdit = new QLineEdit(this);
  m_lineEdit->setPlaceholderText(tr("Search... Use '=' prefix for formula "
                                    "(e.g. =\"Update all\" state:PAUSED)"));
  m_lineEdit->setClearButtonEnabled(true);
  layout->addWidget(m_lineEdit);

  m_treeView = new QTreeView(this);
  m_treeModel = new QStandardItemModel(this);
  m_treeModel->setHorizontalHeaderLabels({tr("Filter Query Structure")});
  m_treeView->setModel(m_treeModel);
  m_treeView->setHeaderHidden(true);
  m_treeView->setDragEnabled(true);
  m_treeView->setAcceptDrops(true);
  m_treeView->setDropIndicatorShown(true);
  m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
  m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

  m_paletteList = new QListWidget(this);
  m_paletteList->setDragEnabled(true);
  m_paletteList->setAcceptDrops(false);
  m_paletteList->setDropIndicatorShown(false);
  m_paletteList->setDefaultDropAction(Qt::CopyAction);

  QStringList paletteItems = {
      QStringLiteral("AND"),           QStringLiteral("OR"),
      QStringLiteral("NOT"),           QStringLiteral("IN"),
      QStringLiteral("state:"),        QStringLiteral("repo:"),
      QStringLiteral("owner:"),        QStringLiteral("created-before:"),
      QStringLiteral("updated-after:")};
  for (const QString &itemText : paletteItems) {
    QListWidgetItem *item = new QListWidgetItem(itemText, m_paletteList);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
                   Qt::ItemIsEnabled);
  }

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(m_treeView);
  splitter->addWidget(m_paletteList);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  splitter->setVisible(false);
  layout->addWidget(splitter);

  connect(m_lineEdit, &QLineEdit::textChanged, this,
          &FilterEditor::onTextChanged);
  connect(m_treeView, &QTreeView::customContextMenuRequested, this,
          &FilterEditor::onTreeContextMenu);
  connect(m_treeModel, &QStandardItemModel::itemChanged, this,
          &FilterEditor::onTreeItemChanged);
  connect(m_treeModel, &QStandardItemModel::rowsInserted, this,
          [this](const QModelIndex &parent, int first, int last) {
            if (m_updating)
              return;
            m_updating = true;
            QStandardItem *parentItem = parent.isValid()
                                            ? m_treeModel->itemFromIndex(parent)
                                            : m_treeModel->invisibleRootItem();
            for (int i = first; i <= last; ++i) {
              QStandardItem *child = parentItem->child(i);
              if (child && !child->data(NodeTypeRole).isValid()) {
                QString text = child->text();
                handleNewItem(child, text);
              }
            }
            m_updating = false;
            updateTextFromTree();
          });
  connect(m_treeModel, &QStandardItemModel::rowsRemoved, this, [this]() {
    if (!m_updating)
      updateTextFromTree();
  });
  connect(m_paletteList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            QString text = item->text();
            QModelIndexList sel =
                m_treeView->selectionModel()->selectedIndexes();
            QStandardItem *parent = m_treeModel->invisibleRootItem();
            if (!sel.isEmpty()) {
              QStandardItem *selItem = m_treeModel->itemFromIndex(sel.first());
              if (selItem && (selItem->data(NodeTypeRole).toInt() == TypeAnd ||
                              selItem->data(NodeTypeRole).toInt() == TypeOr ||
                              selItem->data(NodeTypeRole).toInt() == TypeNot)) {
                parent = selItem;
              } else if (selItem) {
                parent = selItem->parent() ? selItem->parent()
                                           : m_treeModel->invisibleRootItem();
              }
            }
            QStandardItem *newItem = new QStandardItem(text);
            m_updating = true;
            handleNewItem(newItem, text);
            parent->appendRow(newItem);
            m_updating = false;
            updateTextFromTree();
            m_treeView->expandAll();
          });
  connect(m_treeModel, &QStandardItemModel::rowsMoved, this, [this]() {
    if (!m_updating)
      updateTextFromTree();
  });
}

QString FilterEditor::filterText() const { return m_lineEdit->text(); }

void FilterEditor::setCompletions(
    const QMap<QString, QStringList> &completions) {
  m_completions = completions;
}
void FilterEditor::setFilterText(const QString &text) {
  m_lineEdit->setText(text);
}

void FilterEditor::onTextChanged(const QString &text) {
  if (m_updating)
    return;

  m_updating = true;
  if (text.startsWith(QLatin1String("="))) {
    m_treeView->parentWidget()->setVisible(true);
    updateTreeFromText();
  } else {
    m_treeView->parentWidget()->setVisible(false);
    m_treeModel->removeRows(0, m_treeModel->rowCount());
  }
  m_updating = false;

  Q_EMIT filterChanged(text);
}

void FilterEditor::updateTreeFromText() {
  m_treeModel->removeRows(0, m_treeModel->rowCount());
  QSharedPointer<ASTNode> ast = FilterParser::parse(m_lineEdit->text());
  if (ast) {
    populateTree(m_treeModel->invisibleRootItem(), ast);
    m_treeView->expandAll();
  }
}

void FilterEditor::populateTree(QStandardItem *parentItem,
                                QSharedPointer<ASTNode> node) {
  if (!node)
    return;

  QStandardItem *item = new QStandardItem();
  item->setEditable(false);
  item->setDropEnabled(true);
  item->setDragEnabled(true);

  if (auto andNode = qSharedPointerDynamicCast<AndNode>(node)) {
    item->setText(QStringLiteral("AND"));
    item->setData(TypeAnd, NodeTypeRole);
    for (const auto &child : andNode->children()) {
      populateTree(item, child);
    }
  } else if (auto orNode = qSharedPointerDynamicCast<OrNode>(node)) {
    item->setText(QStringLiteral("OR"));
    item->setData(TypeOr, NodeTypeRole);
    for (const auto &child : orNode->children()) {
      populateTree(item, child);
    }
  } else if (auto notNode = qSharedPointerDynamicCast<NotNode>(node)) {
    item->setText(QStringLiteral("NOT"));
    item->setData(TypeNot, NodeTypeRole);
    populateTree(item, notNode->child());
  } else if (auto inNode = qSharedPointerDynamicCast<InNode>(node)) {
    item->setText(inNode->key() + QStringLiteral(" IN \"") +
                  inNode->valuesStr() + QStringLiteral("\""));
    item->setData(TypeIn, NodeTypeRole);
    item->setData(inNode->key(), NodeKeyRole);
    item->setData(inNode->valuesStr(), NodeValueRole);
    item->setEditable(true);
  } else if (auto kvNode = qSharedPointerDynamicCast<KeyValueNode>(node)) {
    item->setText(kvNode->key() + QStringLiteral(":") + kvNode->value());
    item->setData(TypeKV, NodeTypeRole);
    item->setData(kvNode->key(), NodeKeyRole);
    item->setData(kvNode->value(), NodeValueRole);
    item->setEditable(true);
  } else if (auto kwNode = qSharedPointerDynamicCast<KeywordNode>(node)) {
    item->setText(kwNode->keyword());
    item->setData(TypeKeyword, NodeTypeRole);
    item->setData(kwNode->keyword(), NodeValueRole);
    item->setEditable(true);
  }

  parentItem->appendRow(item);
}

void FilterEditor::onTreeContextMenu(const QPoint &pos) {
  QModelIndex index = m_treeView->indexAt(pos);
  if (!index.isValid())
    return;

  QStandardItem *item = m_treeModel->itemFromIndex(index);
  if (!item)
    return;

  QMenu menu(this);
  NodeType type = static_cast<NodeType>(item->data(NodeTypeRole).toInt());

  if (type == TypeAnd) {
    menu.addAction(tr("Switch to OR"), this, [this, item]() {
      item->setData(TypeOr, NodeTypeRole);
      item->setText(QStringLiteral("OR"));
      updateTextFromTree();
    });
  } else if (type == TypeOr) {
    menu.addAction(tr("Switch to AND"), this, [this, item]() {
      item->setData(TypeAnd, NodeTypeRole);
      item->setText(QStringLiteral("AND"));
      updateTextFromTree();
    });
  }

  if (type == TypeKV || type == TypeKeyword || type == TypeIn) {
    menu.addAction(tr("Exclude (NOT this)"), this, [this, item]() {
      QStandardItem *parent =
          item->parent() ? item->parent() : m_treeModel->invisibleRootItem();
      int row = item->row();
      QStandardItem *taken = parent->takeRow(row).first();
      QStandardItem *notItem = new QStandardItem(QStringLiteral("NOT"));
      notItem->setData(TypeNot, NodeTypeRole);
      notItem->appendRow(taken);
      parent->insertRow(row, notItem);
      updateTextFromTree();
    });
  } else if (type == TypeNot) {
    menu.addAction(tr("Include (Remove NOT)"), this, [this, item]() {
      QStandardItem *parent =
          item->parent() ? item->parent() : m_treeModel->invisibleRootItem();
      int row = item->row();
      if (item->rowCount() > 0) {
        QStandardItem *child = item->takeRow(0).first();
        parent->removeRow(row);
        parent->insertRow(row, child);
      } else {
        parent->removeRow(row);
      }
      updateTextFromTree();
    });
  }

  menu.addAction(tr("Delete"), this, [this, item]() {
    QStandardItem *parent =
        item->parent() ? item->parent() : m_treeModel->invisibleRootItem();
    parent->removeRow(item->row());
    updateTextFromTree();
  });

  menu.exec(m_treeView->mapToGlobal(pos));
}

void FilterEditor::onTreeItemChanged(QStandardItem *item) {
  if (m_updating)
    return;

  // User edited the text of a KV, In, or Keyword node.
  NodeType type = static_cast<NodeType>(item->data(NodeTypeRole).toInt());
  QString text = item->text();
  if (type == TypeKV) {
    int idx = text.indexOf(QLatin1Char(':'));
    if (idx > 0) {
      item->setData(text.left(idx), NodeKeyRole);
      item->setData(text.mid(idx + 1), NodeValueRole);
    }
  } else if (type == TypeKeyword) {
    item->setData(text, NodeValueRole);
  } else if (type == TypeIn) {
    int idx = text.indexOf(QStringLiteral(" IN "));
    if (idx > 0) {
      item->setData(text.left(idx), NodeKeyRole);
      QString v = text.mid(idx + 4).trimmed();
      if (v.startsWith(QLatin1Char('"')) && v.endsWith(QLatin1Char('"')))
        v = v.mid(1, v.length() - 2);
      item->setData(v, NodeValueRole);
    }
  }
  updateTextFromTree();
}

bool FilterEditor::handleNewItem(QStandardItem *newItem, const QString &text) {
  if (text == QStringLiteral("AND")) {
    newItem->setData(TypeAnd, NodeTypeRole);
  } else if (text == QStringLiteral("OR")) {
    newItem->setData(TypeOr, NodeTypeRole);
  } else if (text == QStringLiteral("NOT")) {
    newItem->setData(TypeNot, NodeTypeRole);
  } else if (text == QStringLiteral("IN")) {
    newItem->setData(TypeIn, NodeTypeRole);
    newItem->setEditable(true);
    FilterInputDialog dlg(tr("Enter values (comma separated):"), true,
                          QStringList(), QString(), this);
    if (dlg.exec() == QDialog::Accepted) {
      QString key = dlg.getKey();
      QString value = dlg.getValue();
      newItem->setData(key, NodeKeyRole);
      newItem->setData(value, NodeValueRole);
      newItem->setText(key + QStringLiteral(" IN \"") + value +
                       QStringLiteral("\""));
    }
  } else if (text.endsWith(QLatin1Char(':'))) {
    newItem->setData(TypeKV, NodeTypeRole);
    newItem->setEditable(true);
    QString key = text.left(text.length() - 1);
    newItem->setData(key, NodeKeyRole);
    FilterInputDialog dlg(tr("Enter value for ") + key + QStringLiteral(":"),
                          false, m_completions.value(key.toLower()), key, this);
    if (dlg.exec() == QDialog::Accepted) {
      QString value = dlg.getValue();
      newItem->setData(value, NodeValueRole);
      if (value.contains(QLatin1Char(' ')))
        newItem->setText(text + QStringLiteral("\"") + value +
                         QStringLiteral("\""));
      else
        newItem->setText(text + value);
    }
  } else {
    newItem->setData(TypeKeyword, NodeTypeRole);
    newItem->setData(text, NodeValueRole);
    newItem->setEditable(true);
  }
  return true;
}

void FilterEditor::updateTextFromTree() {
  if (m_updating)
    return;

  m_updating = true;
  QStandardItem *rootItem = m_treeModel->invisibleRootItem();
  QStringList parts;
  for (int i = 0; i < rootItem->rowCount(); ++i) {
    QSharedPointer<ASTNode> node = buildASTFromTree(rootItem->child(i));
    if (node)
      parts.append(node->toString());
  }
  QString text = QStringLiteral("=") + parts.join(QLatin1Char(' '));
  m_lineEdit->setText(text);
  m_updating = false;

  Q_EMIT filterChanged(text);
}

QSharedPointer<ASTNode> FilterEditor::buildASTFromTree(QStandardItem *item) {
  if (!item)
    return QSharedPointer<ASTNode>();

  NodeType type = static_cast<NodeType>(item->data(NodeTypeRole).toInt());
  if (type == TypeAnd) {
    QList<QSharedPointer<ASTNode>> children;
    for (int i = 0; i < item->rowCount(); ++i) {
      children.append(buildASTFromTree(item->child(i)));
    }
    return QSharedPointer<ASTNode>(new AndNode(children));
  } else if (type == TypeOr) {
    QList<QSharedPointer<ASTNode>> children;
    for (int i = 0; i < item->rowCount(); ++i) {
      children.append(buildASTFromTree(item->child(i)));
    }
    return QSharedPointer<ASTNode>(new OrNode(children));
  } else if (type == TypeNot) {
    if (item->rowCount() > 0) {
      return QSharedPointer<ASTNode>(
          new NotNode(buildASTFromTree(item->child(0))));
    }
  } else if (type == TypeIn) {
    return QSharedPointer<ASTNode>(
        new InNode(item->data(NodeKeyRole).toString(),
                   item->data(NodeValueRole).toString()));
  } else if (type == TypeKV) {
    return QSharedPointer<ASTNode>(
        new KeyValueNode(item->data(NodeKeyRole).toString(),
                         item->data(NodeValueRole).toString()));
  } else if (type == TypeKeyword) {
    return QSharedPointer<ASTNode>(
        new KeywordNode(item->data(NodeValueRole).toString()));
  }
  return QSharedPointer<ASTNode>();
}

void FilterEditor::setSimplifiedMode(bool simplified) {
  if (simplified) {
    m_paletteList->clear();
    m_paletteList->addItems(
        QStringList{QStringLiteral("OR"), QStringLiteral("AND"),
                    QStringLiteral("NOT"), QStringLiteral("IN"),
                    QStringLiteral("repo:"), QStringLiteral("owner:")});
  } else {
    m_paletteList->clear();
    m_paletteList->addItems(QStringList{
        QStringLiteral("OR"), QStringLiteral("AND"), QStringLiteral("NOT"),
        QStringLiteral("IN"), QStringLiteral("repo:"), QStringLiteral("owner:"),
        QStringLiteral("state:"), QStringLiteral("title:"),
        QStringLiteral("created-before:"), QStringLiteral("created-after:"),
        QStringLiteral("updated-before:"), QStringLiteral("updated-after:")});
  }
}

void FilterEditor::setFocus() { m_lineEdit->setFocus(); }
