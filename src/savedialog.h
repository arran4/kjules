#ifndef SAVEDIALOG_H
#define SAVEDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QTextEdit;

class SaveDialog : public QDialog {
  Q_OBJECT

public:
  // type can be "Template" or "Draft"
  explicit SaveDialog(const QString &type, QWidget *parent = nullptr);

  void setInitialData(const QString &nameOrComment,
                      const QString &description = QString());

  QString nameOrComment() const;
  QString description() const;

private:
  QLineEdit *m_nameEdit;
  QTextEdit *m_descEdit;
  QString m_type;
};

#endif // SAVEDIALOG_H
