#ifndef BACKUPDIALOG_H
#define BACKUPDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class BackupDialog : public QDialog {
  Q_OBJECT
public:
  explicit BackupDialog(const QString &defaultDir, QWidget *parent = nullptr);

  QString backupDirectory() const;
  bool removeOriginals() const;
  bool openDirectory() const;
  QStringList filesToBackup() const;

private Q_SLOTS:
  void browseDirectory();

private:
  QLineEdit *m_dirEdit;
  QCheckBox *m_removeCheckBox;
  QCheckBox *m_openDirCheckBox;

  QCheckBox *m_cbSources;
  QCheckBox *m_cbSessions;
  QCheckBox *m_cbAllSessions;
  QCheckBox *m_cbDrafts;
  QCheckBox *m_cbQueue;
  QCheckBox *m_cbErrors;
};

#endif // BACKUPDIALOG_H
