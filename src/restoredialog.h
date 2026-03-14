#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class RestoreDialog : public QDialog {
  Q_OBJECT
public:
  explicit RestoreDialog(const QString &defaultDir, QWidget *parent = nullptr);

  QString restoreFile() const;
  bool mergeData() const;
  QStringList filesToRestore() const;

private Q_SLOTS:
  void browseFile();

private:
  QLineEdit *m_fileEdit;
  QCheckBox *m_mergeCheckBox;

  QCheckBox *m_cbSources;
  QCheckBox *m_cbSessions;
  QCheckBox *m_cbAllSessions;
  QCheckBox *m_cbDrafts;
  QCheckBox *m_cbQueue;
  QCheckBox *m_cbErrors;
};

#endif // RESTOREDIALOG_H
