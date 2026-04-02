#ifndef FILTEREDITOR_H
#define FILTEREDITOR_H

#include <QSharedPointer>
#include <QWidget>

class QLineEdit;
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class ASTNode;

class FilterEditor : public QWidget {
  Q_OBJECT
public:
  explicit FilterEditor(QWidget *parent = nullptr);
  QString filterText() const;
  void setFilterText(const QString &text);

Q_SIGNALS:
  void filterChanged(const QString &text);

private Q_SLOTS:
  void onTextChanged(const QString &text);
  void onTreeContextMenu(const QPoint &pos);
  void onTreeItemChanged(QStandardItem *item);

private:
  void updateTreeFromText();
  void updateTextFromTree();
  void populateTree(QStandardItem *parentItem, QSharedPointer<ASTNode> node);
  QSharedPointer<ASTNode> buildASTFromTree(QStandardItem *item);

  QLineEdit *m_lineEdit;
  QTreeView *m_treeView;
  QStandardItemModel *m_treeModel;
  bool m_updating;
};

#endif // FILTEREDITOR_H
