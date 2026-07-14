#include "createrepodialog.h"
#include "apimanager.h"
#include <KLocalizedString>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

CreateRepoDialog::CreateRepoDialog(APIManager *apiManager, QWidget *parent)
    : KXmlGuiWindow(parent), m_apiManager(apiManager) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(i18n("Create Repo and Session"));
  resize(800, 600);

  QWidget *centralWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  // Repository settings
  QFormLayout *formLayout = new QFormLayout();

  m_orgEdit = new QLineEdit(this);
  m_orgEdit->setPlaceholderText(
      i18n("Leave blank to use your own user account"));
  formLayout->addRow(i18n("Organization / User:"), m_orgEdit);

  m_repoNameEdit = new QLineEdit(this);
  formLayout->addRow(i18n("Repository Name:"), m_repoNameEdit);

  m_privateCheckBox = new QCheckBox(i18n("Private Repository"), this);
  m_privateCheckBox->setChecked(true);
  formLayout->addRow(QString(), m_privateCheckBox);

  mainLayout->addLayout(formLayout);

  // Session settings
  QLabel *promptLabel = new QLabel(i18n("Session Prompt:"), this);
  mainLayout->addWidget(promptLabel);

  m_promptEdit = new PromptTextEdit(this);
  mainLayout->addWidget(m_promptEdit);

  QHBoxLayout *optionsLayout = new QHBoxLayout();

  m_automationModeComboBox = new QComboBox(this);
  m_automationModeComboBox->addItem(i18n("Auto"), QStringLiteral("auto"));
  m_automationModeComboBox->addItem(i18n("Manual"), QStringLiteral("manual"));
  optionsLayout->addWidget(new QLabel(i18n("Automation Mode:")));
  optionsLayout->addWidget(m_automationModeComboBox);

  m_requirePlanApprovalCheckBox =
      new QCheckBox(i18n("Require Plan Approval"), this);
  optionsLayout->addWidget(m_requirePlanApprovalCheckBox);
  optionsLayout->addStretch();

  mainLayout->addLayout(optionsLayout);

  // Dialog buttons
  QDialogButtonBox *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  m_createButton =
      buttonBox->addButton(i18n("Create"), QDialogButtonBox::AcceptRole);
  connect(buttonBox, &QDialogButtonBox::accepted, this,
          &CreateRepoDialog::onSubmit);
  connect(buttonBox, &QDialogButtonBox::rejected, this,
          &CreateRepoDialog::close);
  mainLayout->addWidget(buttonBox);

  setCentralWidget(centralWidget);

  QStatusBar *statusBar = new QStatusBar(this);
  setStatusBar(statusBar);

  connect(m_apiManager, &APIManager::githubUsernameFetched, this,
          &CreateRepoDialog::onGithubUsernameFetched);
  connect(m_apiManager, &APIManager::githubConnectionTested, this,
          [this](bool success, const QString &message) {
            if (!success) {
              updateStatus(i18n("GitHub check failed: %1", message));
              m_createButton->setEnabled(true);
            } else {
              QString scopes = m_apiManager->githubScopes();
              if (scopes.isEmpty() ||
                  (!scopes.contains(QStringLiteral("repo")) &&
                   !scopes.contains(QStringLiteral("public_repo")))) {
                updateStatus(i18n("Warning: GitHub token may lack 'repo' scope "
                                  "required to create repositories."));
              } else {
                updateStatus(i18n("Ready."));
              }
              m_createButton->setEnabled(true);
            }
          });
}

void CreateRepoDialog::showEvent(QShowEvent *event) {
  KXmlGuiWindow::showEvent(event);
  if (m_apiManager->githubToken().isEmpty()) {
    updateStatus(i18n("Missing GitHub token. Please add one in Settings."));
    m_createButton->setEnabled(false);
  } else {
    updateStatus(i18n("Checking GitHub connection..."));
    m_createButton->setEnabled(false);
    m_apiManager->testGithubConnection();
  }
}

void CreateRepoDialog::onGithubUsernameFetched(const QString &username) {
  Q_UNUSED(username);
}

void CreateRepoDialog::updateStatus(const QString &message) {
  statusBar()->showMessage(message);
}

void CreateRepoDialog::onSubmit() {
  QString repoName = m_repoNameEdit->text().trimmed();
  if (repoName.isEmpty()) {
    updateStatus(i18n("Repository name cannot be empty."));
    return;
  }

  QString org = m_orgEdit->text().trimmed();
  if (org.isEmpty() && m_apiManager->githubUsername().isEmpty()) {
    updateStatus(i18n("Organization / User cannot be empty if GitHub username "
                      "could not be fetched."));
    return;
  }
  bool isPrivate = m_privateCheckBox->isChecked();
  QString prompt = m_promptEdit->getPromptText();
  QString automationMode = m_automationModeComboBox->currentData().toString();
  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  Q_EMIT createRepoAndSessionRequested(org, repoName, isPrivate, prompt,
                                       automationMode, requirePlanApproval);
  close();
}
