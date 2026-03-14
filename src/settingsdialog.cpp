#include "settingsdialog.h"
#include "apimanager.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

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

  KConfigGroup config(KSharedConfig::openConfig(), "General");
  m_closeToTrayEdit = new QCheckBox(i18n("Close to Tray"), this);
  m_closeToTrayEdit->setChecked(config.readEntry("CloseToTray", false));
  formLayout->addRow(QString(), m_closeToTrayEdit);

  KConfigGroup queueConfig(KSharedConfig::openConfig(), "Queue");

  m_queueIntervalEdit = new QSpinBox(this);
  m_queueIntervalEdit->setRange(1, 1440); // 1 min to 24 hours
  m_queueIntervalEdit->setSuffix(i18n(" minutes"));
  m_queueIntervalEdit->setValue(queueConfig.readEntry("TimerInterval", 1));
  formLayout->addRow(i18n("Queue processing interval:"), m_queueIntervalEdit);

  m_queueBackoffEdit = new QSpinBox(this);
  m_queueBackoffEdit->setRange(1, 10080); // 1 min to 1 week
  m_queueBackoffEdit->setSuffix(i18n(" minutes"));
  m_queueBackoffEdit->setValue(queueConfig.readEntry("BackoffInterval", 30));
  formLayout->addRow(i18n("Queue failure backoff:"), m_queueBackoffEdit);

  m_tierComboBox = new QComboBox(this);
  m_tierComboBox->addItem(i18n("Free (3 jobs)"), QStringLiteral("free"));
  m_tierComboBox->addItem(i18n("Pro (15 jobs)"), QStringLiteral("pro"));
  m_tierComboBox->addItem(i18n("Max (30 jobs)"), QStringLiteral("max"));

  QString currentTier = config.readEntry("Tier", QStringLiteral("free"));
  int index = m_tierComboBox->findData(currentTier);
  if (index >= 0) {
    m_tierComboBox->setCurrentIndex(index);
  }
  formLayout->addRow(i18n("Account Tier:"), m_tierComboBox);

  KConfigGroup sessionConfig(KSharedConfig::openConfig(), "SessionWindow");
  m_globalAutoRefreshCombo = new QComboBox(this);
  m_globalAutoRefreshCombo->addItem(i18n("Off"), 0);
  m_globalAutoRefreshCombo->addItem(i18n("10 seconds"), 10);
  m_globalAutoRefreshCombo->addItem(i18n("30 seconds"), 30);
  m_globalAutoRefreshCombo->addItem(i18n("1 minute"), 60);
  m_globalAutoRefreshCombo->addItem(i18n("5 minutes"), 300);
  m_globalAutoRefreshCombo->setCurrentIndex(
      sessionConfig.readEntry("AutoRefreshIndex", 0));
  formLayout->addRow(i18n("Default session auto-refresh:"),
                     m_globalAutoRefreshCombo);

  mainLayout->addLayout(formLayout);

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *testButton = new QPushButton(i18n("Test API Key"), this);
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

  KConfigGroup config(KSharedConfig::openConfig(), "General");
  config.writeEntry("CloseToTray", m_closeToTrayEdit->isChecked());
  config.writeEntry("Tier", m_tierComboBox->currentData().toString());
  config.sync();

  KConfigGroup queueConfig(KSharedConfig::openConfig(), "Queue");
  queueConfig.writeEntry("TimerInterval", m_queueIntervalEdit->value());
  queueConfig.writeEntry("BackoffInterval", m_queueBackoffEdit->value());
  queueConfig.sync();

  KConfigGroup sessionConfig(KSharedConfig::openConfig(), "SessionWindow");
  sessionConfig.writeEntry("AutoRefreshIndex",
                           m_globalAutoRefreshCombo->currentIndex());
  sessionConfig.sync();

  accept();
}
