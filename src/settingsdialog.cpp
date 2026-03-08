#include "settingsdialog.h"
#include "apimanager.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(APIManager *apiManager, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager) {
  setWindowTitle(i18n("Settings"));
  resize(500, 250);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QFormLayout *formLayout = new QFormLayout();

  // API Key Row
  m_apiKeyEdit = new QLineEdit(this);
  m_apiKeyEdit->setText(m_apiManager->apiKey());
  m_apiKeyEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);

  QHBoxLayout *apiKeyLayout = new QHBoxLayout();
  apiKeyLayout->addWidget(m_apiKeyEdit);

  QPushButton *testApiKeyBtn = new QPushButton(i18n("Test API Key"), this);
  connect(testApiKeyBtn, &QPushButton::clicked, this,
          &SettingsDialog::onTestApiKey);
  apiKeyLayout->addWidget(testApiKeyBtn);

  m_apiKeyStatus = new QLabel(this);
  apiKeyLayout->addWidget(m_apiKeyStatus);

  formLayout->addRow(i18n("Google Jules API Key:"), apiKeyLayout);

  // GitHub Token Row
  m_githubTokenEdit = new QLineEdit(this);
  m_githubTokenEdit->setText(m_apiManager->githubToken());
  m_githubTokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);

  QHBoxLayout *githubTokenLayout = new QHBoxLayout();
  githubTokenLayout->addWidget(m_githubTokenEdit);

  QPushButton *testGithubTokenBtn = new QPushButton(i18n("Test Token"), this);
  connect(testGithubTokenBtn, &QPushButton::clicked, this,
          &SettingsDialog::onTestGithubToken);
  githubTokenLayout->addWidget(testGithubTokenBtn);

  m_githubTokenStatus = new QLabel(this);
  githubTokenLayout->addWidget(m_githubTokenStatus);

  formLayout->addRow(i18n("GitHub Token:"), githubTokenLayout);

  // Backoff Setting
  m_tierComboBox = new QComboBox(this);
  m_tierComboBox->addItem(i18n("Free (3 jobs)"), 0);
  m_tierComboBox->addItem(i18n("Pro (15 jobs)"), 1);
  m_tierComboBox->addItem(i18n("Max (30 jobs)"), 2);

  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup settingsGroup(config, "Settings");
  int currentTier = settingsGroup.readEntry("BackoffTier", 0);
  m_tierComboBox->setCurrentIndex(currentTier);

  formLayout->addRow(i18n("Backoff Tier:"), m_tierComboBox);

  mainLayout->addLayout(formLayout);

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *saveButton = new QPushButton(i18n("Save"), this);
  saveButton->setDefault(true);
  connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);

  QPushButton *cancelButton = new QPushButton(i18n("Cancel"), this);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  buttonLayout->addStretch();
  buttonLayout->addWidget(saveButton);
  buttonLayout->addWidget(cancelButton);

  mainLayout->addLayout(buttonLayout);

  connect(m_apiManager, &APIManager::connectionTested, this,
          [this](bool success, const QString &message) {
            if (success) {
              m_apiKeyStatus->setText(i18n("<font color='green'>OK</font>"));
            } else {
              m_apiKeyStatus->setText(i18n("<font color='red'>Failed</font>"));
              m_apiKeyStatus->setToolTip(message);
            }
          });
}

void SettingsDialog::onTestApiKey() {
  m_apiKeyStatus->setText(i18n("Testing..."));
  m_apiManager->testConnection(m_apiKeyEdit->text());
}

void SettingsDialog::onTestGithubToken() {
  if (!m_githubTokenEdit->text().isEmpty()) {
    m_githubTokenStatus->setText(i18n("<font color='green'>OK</font>"));
  } else {
    m_githubTokenStatus->setText(i18n("<font color='red'>Empty</font>"));
  }
}

void SettingsDialog::onSave() {
  m_apiManager->setApiKey(m_apiKeyEdit->text());
  m_apiManager->setGithubToken(m_githubTokenEdit->text());

  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup settingsGroup(config, "Settings");
  settingsGroup.writeEntry("BackoffTier", m_tierComboBox->currentIndex());
  config->sync();

  Q_EMIT tierChanged(m_tierComboBox->currentIndex());

  accept();
}
