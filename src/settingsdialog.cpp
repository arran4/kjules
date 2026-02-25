#include "settingsdialog.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

#include "apimanager.h"

SettingsDialog::SettingsDialog(APIManager *apiManager, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager) {
  setWindowTitle(i18n("Settings"));
  resize(400, 200);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QFormLayout *formLayout = new QFormLayout();

  m_apiKeyEdit = new QLineEdit(this);
  m_apiKeyEdit->setText(m_apiManager->apiKey());
  m_apiKeyEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
  formLayout->addRow(i18n("Google Jules API Key:"), m_apiKeyEdit);

  m_githubTokenEdit = new QLineEdit(this);
  m_githubTokenEdit->setText(m_apiManager->githubToken());
  m_githubTokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
  formLayout->addRow(i18n("GitHub Token (Optional):"), m_githubTokenEdit);

  m_autostartCheck = new QCheckBox(i18n("Start on Login"), this);
  formLayout->addRow("", m_autostartCheck);

  mainLayout->addLayout(formLayout);

  loadSettings();

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *testButton = new QPushButton(i18n("Test Connection"), this);
  connect(testButton, &QPushButton::clicked, this,
          &SettingsDialog::onTestConnection);

  QPushButton *saveButton = new QPushButton(i18n("Save"), this);
  saveButton->setDefault(true);
  connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);

  QPushButton *cancelButton = new QPushButton(i18n("Cancel"), this);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  buttonLayout->addWidget(testButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(saveButton);
  buttonLayout->addWidget(cancelButton);

  mainLayout->addLayout(buttonLayout);

  connect(m_apiManager, &APIManager::connectionTested, this,
          [this](bool success, const QString &message) {
            if (success) {
              QMessageBox::information(this, i18n("Connection Test"), message);
            } else {
              QMessageBox::warning(this, i18n("Connection Test"), message);
            }
          });
}

void SettingsDialog::onTestConnection() {
  m_apiManager->testConnection(m_apiKeyEdit->text());
}

void SettingsDialog::onSave() {
  m_apiManager->setApiKey(m_apiKeyEdit->text());
  m_apiManager->setGithubToken(m_githubTokenEdit->text());
  updateAutostart(m_autostartCheck->isChecked());
  accept();
}

void SettingsDialog::loadSettings() {
  QString configPath =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  QString autostartPath = configPath + "/autostart/org.kde.kjules.desktop";
  m_autostartCheck->setChecked(QFile::exists(autostartPath));
}

void SettingsDialog::updateAutostart(bool enable) {
  QString configPath =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  QDir autostartDir(configPath + "/autostart");
  if (!autostartDir.exists()) {
    autostartDir.mkpath(".");
  }
  QString autostartPath = autostartDir.filePath("org.kde.kjules.desktop");

  if (enable) {
    if (!QFile::exists(autostartPath)) {
      QFile file(autostartPath);
      if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=kJules\n";
        out << "Exec=kjules\n";
        out << "Icon=sc-apps-kjules\n";
        out << "X-KDE-StartupNotify=false\n";
        out << "Terminal=false\n";
        out << "Categories=Development;\n";
        file.close();
      }
    }
  } else {
    if (QFile::exists(autostartPath)) {
      QFile::remove(autostartPath);
    }
  }
}
