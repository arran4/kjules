#include "settingsdialog.h"
#include "apimanager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <KLocalizedString>

SettingsDialog::SettingsDialog(APIManager *apiManager, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager)
{
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

    mainLayout->addLayout(formLayout);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    QPushButton *testButton = new QPushButton(i18n("Test Connection"), this);
    connect(testButton, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);

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

    connect(m_apiManager, &APIManager::connectionTested, this, [this](bool success, const QString &message) {
        if (success) {
            QMessageBox::information(this, i18n("Connection Test"), message);
        } else {
            QMessageBox::warning(this, i18n("Connection Test"), message);
        }
    });
}

void SettingsDialog::onTestConnection()
{
    m_apiManager->testConnection(m_apiKeyEdit->text());
}

void SettingsDialog::onSave()
{
    m_apiManager->setApiKey(m_apiKeyEdit->text());
    m_apiManager->setGithubToken(m_githubTokenEdit->text());
    accept();
}
