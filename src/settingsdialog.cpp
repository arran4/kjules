#include "settingsdialog.h"
#include "apimanager.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
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
  m_queueIntervalSpin = new QSpinBox(this);
  m_queueIntervalSpin->setRange(1, 3600);
  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup group = config->group("General");
  m_queueIntervalSpin->setValue(group.readEntry("QueueInterval", 60));
  formLayout->addRow(i18n("Queue Processing Interval (seconds):"),
                     m_queueIntervalSpin);

  KConfigGroup config(KSharedConfig::openConfig(), "General");
  m_closeToTrayEdit = new QCheckBox(i18n("Close to Tray"), this);
  m_closeToTrayEdit->setChecked(config.readEntry("CloseToTray", false));
  formLayout->addRow(QString(), m_closeToTrayEdit);

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

  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup group(config, "General");
  group.writeEntry("CloseToTray", m_closeToTrayEdit->isChecked());
  group.writeEntry("Tier", m_tierComboBox->currentData().toString());
  group.writeEntry("QueueInterval", m_queueIntervalSpin->value());
  config.sync();

  accept();
}
