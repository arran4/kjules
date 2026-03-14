#include "restoredialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

RestoreDialog::RestoreDialog(const QString &defaultDir, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(i18n("Restore Data"));
  setModal(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Explanatory text
  QLabel *infoLabel =
      new QLabel(i18n("This will restore your local data from a zip file. "
                      "Please select the backup file and options below."),
                 this);
  infoLabel->setWordWrap(true);
  mainLayout->addWidget(infoLabel);

  // File selection
  QHBoxLayout *fileLayout = new QHBoxLayout();
  m_fileEdit = new QLineEdit(defaultDir, this);
  QPushButton *browseBtn = new QPushButton(i18n("Browse..."), this);
  connect(browseBtn, &QPushButton::clicked, this,
          &RestoreDialog::browseFile);
  fileLayout->addWidget(new QLabel(i18n("Backup File:"), this));
  fileLayout->addWidget(m_fileEdit);
  fileLayout->addWidget(browseBtn);
  mainLayout->addLayout(fileLayout);

  // Checkboxes for files
  QLabel *filesLabel = new QLabel(i18n("Files to restore:"), this);
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
  m_mergeCheckBox =
      new QCheckBox(i18n("Merge with existing data (overwrite if unchecked)"), this);
  m_mergeCheckBox->setChecked(true);
  mainLayout->addWidget(m_mergeCheckBox);

  // Buttons
  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);

  resize(450, 400);
}

void RestoreDialog::browseFile() {
  QString file = QFileDialog::getOpenFileName(
      this, i18n("Select Backup File"), m_fileEdit->text(), i18n("Zip Archives (*.zip)"));
  if (!file.isEmpty()) {
    m_fileEdit->setText(file);
  }
}

QString RestoreDialog::restoreFile() const { return m_fileEdit->text(); }

bool RestoreDialog::mergeData() const {
  return m_mergeCheckBox->isChecked();
}

QStringList RestoreDialog::filesToRestore() const {
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
