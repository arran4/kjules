#include "backupdialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

BackupDialog::BackupDialog(const QString &defaultDir, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(i18n("Backup Data"));
  setModal(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Explanatory text
  QLabel *infoLabel =
      new QLabel(i18n("This will archive your local data into a zip file. "
                      "Please select the backup location and options below."),
                 this);
  infoLabel->setWordWrap(true);
  mainLayout->addWidget(infoLabel);

  // Directory selection
  QHBoxLayout *dirLayout = new QHBoxLayout();
  m_dirEdit = new QLineEdit(defaultDir, this);
  QPushButton *browseBtn = new QPushButton(i18n("Browse..."), this);
  connect(browseBtn, &QPushButton::clicked, this,
          &BackupDialog::browseDirectory);
  dirLayout->addWidget(new QLabel(i18n("Backup Location:"), this));
  dirLayout->addWidget(m_dirEdit);
  dirLayout->addWidget(browseBtn);
  mainLayout->addLayout(dirLayout);

  // Checkboxes for files
  QLabel *filesLabel = new QLabel(i18n("Files to backup:"), this);
  mainLayout->addWidget(filesLabel);

  QFormLayout *filesLayout = new QFormLayout();
  m_cbSources = new QCheckBox(i18n("Sources (sources.json)"), this);
  m_cbSources->setChecked(true);
  filesLayout->addRow(m_cbSources);

  m_cbSessions =
      new QCheckBox(i18n("Cached Sessions (cached_sessions.json)"), this);
  m_cbSessions->setChecked(true);
  filesLayout->addRow(m_cbSessions);

  m_cbAllSessions =
      new QCheckBox(i18n("All Sessions (cached_all_sessions.json)"), this);
  m_cbAllSessions->setChecked(true);
  filesLayout->addRow(m_cbAllSessions);

  m_cbDrafts = new QCheckBox(i18n("Drafts (drafts.json)"), this);
  m_cbDrafts->setChecked(true);
  filesLayout->addRow(m_cbDrafts);

  m_cbQueue = new QCheckBox(i18n("Queue (queue.json)"), this);
  m_cbQueue->setChecked(true);
  filesLayout->addRow(m_cbQueue);

  m_cbErrors = new QCheckBox(i18n("Errors (errors.json)"), this);
  m_cbErrors->setChecked(true);
  filesLayout->addRow(m_cbErrors);

  mainLayout->addLayout(filesLayout);

  // Options
  m_removeCheckBox =
      new QCheckBox(i18n("Remove original files after backup"), this);
  m_removeCheckBox->setChecked(false); // Dangerous option, default off
  mainLayout->addWidget(m_removeCheckBox);

  m_openDirCheckBox =
      new QCheckBox(i18n("Open directory containing backup afterwards"), this);
  m_openDirCheckBox->setChecked(true);
  mainLayout->addWidget(m_openDirCheckBox);

  // Buttons
  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);

  resize(450, 400);
}

void BackupDialog::browseDirectory() {
  QString dir = QFileDialog::getExistingDirectory(
      this, i18n("Select Backup Directory"), m_dirEdit->text());
  if (!dir.isEmpty()) {
    m_dirEdit->setText(dir);
  }
}

QString BackupDialog::backupDirectory() const { return m_dirEdit->text(); }

bool BackupDialog::removeOriginals() const {
  return m_removeCheckBox->isChecked();
}

bool BackupDialog::openDirectory() const {
  return m_openDirCheckBox->isChecked();
}

QStringList BackupDialog::filesToBackup() const {
  QStringList files;
  if (m_cbSources->isChecked())
    files.append(QStringLiteral("sources.json"));
  if (m_cbSessions->isChecked())
    files.append(QStringLiteral("cached_sessions.json"));
  if (m_cbAllSessions->isChecked())
    files.append(QStringLiteral("cached_all_sessions.json"));
  if (m_cbDrafts->isChecked())
    files.append(QStringLiteral("drafts.json"));
  if (m_cbQueue->isChecked())
    files.append(QStringLiteral("queue.json"));
  if (m_cbErrors->isChecked())
    files.append(QStringLiteral("errors.json"));
  return files;
}
