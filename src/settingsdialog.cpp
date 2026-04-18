#include "settingsdialog.h"
#include "apimanager.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QTabWidget>

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

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  m_closeToTrayEdit = new QCheckBox(i18n("Close to Tray"), this);
  m_closeToTrayEdit->setChecked(config.readEntry("CloseToTray", false));
  formLayout->addRow(QString(), m_closeToTrayEdit);

  m_autostartEdit = new QCheckBox(i18n("Open automatically upon login"), this);
  m_autostartEdit->setChecked(config.readEntry("Autostart", false));
  formLayout->addRow(QString(), m_autostartEdit);

  m_autostartTrayEdit = new QCheckBox(
      i18n("When opening automatically start in system tray"), this);
  m_autostartTrayEdit->setChecked(config.readEntry("AutostartTray", false));
  formLayout->addRow(QString(), m_autostartTrayEdit);

  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));

  m_queueIntervalEdit = new QSpinBox(this);
  m_queueIntervalEdit->setRange(1, 1440); // 1 min to 24 hours
  m_queueIntervalEdit->setSuffix(i18n(" minutes"));
  m_queueIntervalEdit->setValue(queueConfig.readEntry("TimerInterval", 1));
  formLayout->addRow(i18n("Queue processing interval:"), m_queueIntervalEdit);

  m_backoffTabWidget = new QTabWidget(this);

  // Fixed Tab
  QWidget *fixedTab = new QWidget();
  QFormLayout *fixedLayout = new QFormLayout(fixedTab);
  m_queueBackoffEdit = new QSpinBox(this);
  m_queueBackoffEdit->setRange(1, 10080); // 1 min to 1 week
  m_queueBackoffEdit->setSuffix(i18n(" minutes"));
  m_queueBackoffEdit->setValue(queueConfig.readEntry("BackoffInterval", 30));
  fixedLayout->addRow(i18n("Queue failure fixed/initial backoff:"), m_queueBackoffEdit);
  m_backoffTabWidget->addTab(fixedTab, i18n("Fixed"));

  // Exponential Tab
  QWidget *expTab = new QWidget();
  QFormLayout *expLayout = new QFormLayout(expTab);
  m_queueBackoffExpBaseEdit = new QSpinBox(this);
  m_queueBackoffExpBaseEdit->setRange(1, 100);
  m_queueBackoffExpBaseEdit->setValue(queueConfig.readEntry("BackoffExpBase", 2));
  expLayout->addRow(i18n("Queue failure exponential base:"), m_queueBackoffExpBaseEdit);
  m_backoffTabWidget->addTab(expTab, i18n("Exponential"));

  // Random Tab
  QWidget *randomTab = new QWidget();
  QFormLayout *randomLayout = new QFormLayout(randomTab);
  m_queueBackoffRandomMinEdit = new QSpinBox(this);
  m_queueBackoffRandomMinEdit->setRange(1, 10080);
  m_queueBackoffRandomMinEdit->setSuffix(i18n(" minutes"));
  m_queueBackoffRandomMinEdit->setValue(queueConfig.readEntry("BackoffRandomMin", 10));
  randomLayout->addRow(i18n("Queue failure random min:"), m_queueBackoffRandomMinEdit);

  m_queueBackoffRandomMaxEdit = new QSpinBox(this);
  m_queueBackoffRandomMaxEdit->setRange(1, 10080);
  m_queueBackoffRandomMaxEdit->setSuffix(i18n(" minutes"));
  m_queueBackoffRandomMaxEdit->setValue(queueConfig.readEntry("BackoffRandomMax", 60));
  randomLayout->addRow(i18n("Queue failure random max:"), m_queueBackoffRandomMaxEdit);
  m_backoffTabWidget->addTab(randomTab, i18n("Random"));

  // Determine current tab
  QString currentBackoffType = queueConfig.readEntry("BackoffType", QStringLiteral("fixed"));
  int backoffTypeIndex = 0;
  if (currentBackoffType == QStringLiteral("exponential")) {
    backoffTypeIndex = 1;
  } else if (currentBackoffType == QStringLiteral("random")) {
    backoffTypeIndex = 2;
  }
  m_backoffTabWidget->setCurrentIndex(backoffTypeIndex);

  // Note: Qt doesn't directly support HTML bolding or setTabFont for individual tabs
  // natively in all styles without custom painting, but appending a marker works.
  auto updateTabTitles = [this](int index) {
    for (int i = 0; i < m_backoffTabWidget->count(); ++i) {
      QString text = m_backoffTabWidget->tabText(i);
      text.remove(QStringLiteral(" (Selected)"));
      if (i == index) {
        text += QStringLiteral(" (Selected)");
      }
      m_backoffTabWidget->setTabText(i, text);
    }
  };
  connect(m_backoffTabWidget, &QTabWidget::currentChanged, this, updateTabTitles);
  updateTabTitles(backoffTypeIndex);

  formLayout->addRow(i18n("Backoff Strategy:"), m_backoffTabWidget);

  m_queueBackoffMaxEdit = new QSpinBox(this);
  m_queueBackoffMaxEdit->setRange(1, 10080); // Up to 1 week
  m_queueBackoffMaxEdit->setSuffix(i18n(" minutes"));
  m_queueBackoffMaxEdit->setValue(queueConfig.readEntry("BackoffMax", 480)); // Default 8 hours
  formLayout->addRow(i18n("Global maximum backoff:"), m_queueBackoffMaxEdit);

  m_waitTimeEdit = new QSpinBox(this);
  m_waitTimeEdit->setRange(1, 10080);
  m_waitTimeEdit->setSuffix(i18n(" minutes"));
  m_waitTimeEdit->setValue(config.readEntry("WaitTime", 3600) / 60);
  formLayout->addRow(i18n("Queue concurrency wait:"), m_waitTimeEdit);

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

  KConfigGroup sessionConfig(KSharedConfig::openConfig(),
                             QStringLiteral("SessionWindow"));
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

  m_autoArchiveCheckbox = new QCheckBox(
      i18n("Automatically archive following managed sessions"), this);
  m_autoArchiveCheckbox->setChecked(
      sessionConfig.readEntry("AutoArchiveEnabled", true));
  formLayout->addRow(QString(), m_autoArchiveCheckbox);

  m_autoArchiveDaysEdit = new QSpinBox(this);
  m_autoArchiveDaysEdit->setRange(1, 3650);
  m_autoArchiveDaysEdit->setSuffix(i18n(" days after creation"));
  m_autoArchiveDaysEdit->setValue(
      sessionConfig.readEntry("AutoArchiveDays", 30));
  formLayout->addRow(i18n("Archive after:"), m_autoArchiveDaysEdit);

  m_prMergeArchiveCheckbox =
      new QCheckBox(i18n("When a following job has a PR that is merged move it "
                         "to the archive"),
                    this);
  m_prMergeArchiveCheckbox->setChecked(
      sessionConfig.readEntry("PrMergeArchiveEnabled", true));
  formLayout->addRow(QString(), m_prMergeArchiveCheckbox);

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

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  config.writeEntry("CloseToTray", m_closeToTrayEdit->isChecked());
  config.writeEntry("Autostart", m_autostartEdit->isChecked());
  config.writeEntry("AutostartTray", m_autostartTrayEdit->isChecked());
  config.writeEntry("Tier", m_tierComboBox->currentData().toString());
  config.writeEntry("WaitTime", m_waitTimeEdit->value() * 60);
  config.sync();

  QString autostartPath =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
      QStringLiteral("/autostart");
  QDir dir(autostartPath);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QString desktopFilePath =
      dir.filePath(QStringLiteral("org.kde.kjules.desktop"));

  if (m_autostartEdit->isChecked()) {
    QFile file(desktopFilePath);
    if (file.open(QIODevice::WriteOnly)) {
      file.write("[Desktop Entry]\n"
                 "Name=kJules\n"
                 "Exec=kjules --autostarted\n"
                 "Icon=sc-apps-kjules\n"
                 "Type=Application\n");
      file.close();
    }
  } else {
    QFile::remove(desktopFilePath);
  }

  KConfigGroup queueConfig(KSharedConfig::openConfig(),
                           QStringLiteral("Queue"));
  queueConfig.writeEntry("TimerInterval", m_queueIntervalEdit->value());
  QString backoffType = QStringLiteral("fixed");
  if (m_backoffTabWidget->currentIndex() == 1) {
    backoffType = QStringLiteral("exponential");
  } else if (m_backoffTabWidget->currentIndex() == 2) {
    backoffType = QStringLiteral("random");
  }
  queueConfig.writeEntry("BackoffType", backoffType);
  queueConfig.writeEntry("BackoffInterval", m_queueBackoffEdit->value());
  queueConfig.writeEntry("BackoffExpBase", m_queueBackoffExpBaseEdit->value());
  queueConfig.writeEntry("BackoffRandomMin", m_queueBackoffRandomMinEdit->value());
  queueConfig.writeEntry("BackoffRandomMax", m_queueBackoffRandomMaxEdit->value());
  queueConfig.writeEntry("BackoffMax", m_queueBackoffMaxEdit->value());
  queueConfig.sync();

  KConfigGroup sessionConfig(KSharedConfig::openConfig(),
                             QStringLiteral("SessionWindow"));
  sessionConfig.writeEntry("AutoRefreshIndex",
                           m_globalAutoRefreshCombo->currentIndex());
  sessionConfig.writeEntry("AutoArchiveEnabled",
                           m_autoArchiveCheckbox->isChecked());
  sessionConfig.writeEntry("AutoArchiveDays", m_autoArchiveDaysEdit->value());
  sessionConfig.writeEntry("PrMergeArchiveEnabled",
                           m_prMergeArchiveCheckbox->isChecked());
  sessionConfig.sync();

  accept();
}
