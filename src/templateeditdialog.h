#ifndef TEMPLATEEDITDIALOG_H
#define TEMPLATEEDITDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QLineEdit;
class QTextEdit;
class QCheckBox;

class TemplateEditDialog : public QDialog {
  Q_OBJECT

public:
  explicit TemplateEditDialog(QWidget *parent = nullptr);

  void setInitialData(const QJsonObject &data);
  QJsonObject templateData() const;

private:
  QLineEdit *m_nameEdit;
  QTextEdit *m_descEdit;
  QTextEdit *m_promptEdit;
  QCheckBox *m_requirePlanApprovalCheckBox;
  QJsonObject m_originalData;
};

#endif // TEMPLATEEDITDIALOG_H
