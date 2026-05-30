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
class QFrame;
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

  static QString applyQuickFilter(const QString &currentFilter,
                                  const QString &type, const QString &value,
                                  bool isHide);

public Q_SLOTS:
  void focusInput();

Q_SIGNALS:
  void filterChanged(const QString &text);
  void returnPressed();

private Q_SLOTS:
  void onTextChanged(const QString &text);
  void onTreeContextMenu(const QPoint &pos);
  void onTreeItemChanged(QStandardItem *item);
  void toggleFormulaBuilder();
  void dismissFormulaBuilder();

private:
  void updateTreeFromText();
  bool handleNewItem(QStandardItem *newItem, const QString &text);
  void updateTextFromTree();
  void populateTree(QStandardItem *parentItem, QSharedPointer<ASTNode> node);
  QSharedPointer<ASTNode> buildASTFromTree(QStandardItem *item);
  void updatePopupPosition();

  QLineEdit *m_lineEdit;
  QToolButton *m_toggleButton;
  QTreeView *m_treeView;
  QStandardItemModel *m_treeModel;
  QListWidget *m_paletteList;
  QFrame *m_popupFrame;
  bool m_updating;
  bool m_userDismissed;
  QMap<QString, QStringList> m_completions;

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // FILTEREDITOR_H
