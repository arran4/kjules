#ifndef FILTEREDITOR_H
#define FILTEREDITOR_H

#include <QMap>
#include <QSharedPointer>
#include <QWidget>

class QLineEdit;
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QListWidget;
class QToolButton;
class QListWidget;
class QToolButton;
class ASTNode;

class FilterEditor : public QWidget {
  Q_OBJECT
public:
  explicit FilterEditor(QWidget *parent = nullptr);
  QString filterText() const;
  QLineEdit *lineEdit() const;
  void setFilterText(const QString &text);
  void setCompletions(const QMap<QString, QStringList> &completions);
  void setSimplifiedMode(bool simplified);
  void toggleFormulaBuilder();
  void setFormulaBuilderVisible(bool visible);

Q_SIGNALS:
  void filterChanged(const QString &text);
  void returnPressed();

private Q_SLOTS:
  void onTextChanged(const QString &text);
  void onTreeContextMenu(const QPoint &pos);
  void onTreeItemChanged(QStandardItem *item);

private:
  void updateTreeFromText();
  bool handleNewItem(QStandardItem *newItem, const QString &text);
  void updateTextFromTree();
  void populateTree(QStandardItem *parentItem, QSharedPointer<ASTNode> node);
  QSharedPointer<ASTNode> buildASTFromTree(QStandardItem *item);

  QLineEdit *m_lineEdit;
  QTreeView *m_treeView;
  QStandardItemModel *m_treeModel;
  QListWidget *m_paletteList;
  bool m_updating;
  bool m_builderForceHidden;
  QToolButton *m_formulaToggleBtn;
  QMap<QString, QStringList> m_completions;
};

#endif // FILTEREDITOR_H
