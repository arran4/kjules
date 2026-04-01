#ifndef TEMPLATESELECTIONDIALOG_H
#define TEMPLATESELECTIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QLineEdit;
class QListView;
class QSortFilterProxyModel;
class TemplatesModel;

class TemplateSelectionDialog : public QDialog {
  Q_OBJECT

public:
  explicit TemplateSelectionDialog(TemplatesModel *templatesModel,
                                   QWidget *parent = nullptr);

  QJsonObject selectedTemplate() const;

private Q_SLOTS:
  void onSelectionChanged();
  void onDoubleClicked();

private:
  TemplatesModel *m_templatesModel;
  QSortFilterProxyModel *m_proxyModel;
  QLineEdit *m_filterEdit;
  QListView *m_listView;
  QPushButton *m_selectButton;

  QJsonObject m_selectedTemplate;
};

#endif // TEMPLATESELECTIONDIALOG_H
