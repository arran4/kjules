#include "followsessiondialog.h"
#include "apimanager.h"
#include "sessionmodel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <KLocalizedString>
#include <QEventLoop>

FollowSessionDialog::FollowSessionDialog(APIManager *apiManager, SessionModel *sessionModel, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager), m_sessionModel(sessionModel) {
  setWindowTitle(i18n("Follow from Jules Session ID"));

  QVBoxLayout *layout = new QVBoxLayout(this);

  QLabel *infoLabel = new QLabel(i18n("Enter Jules Session ID or URL:"), this);
  layout->addWidget(infoLabel);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  m_idEdit = new QLineEdit(this);
  m_idEdit->setPlaceholderText(QStringLiteral("e.g. 14074060995680401415 or https://..."));
  inputLayout->addWidget(m_idEdit);

  m_previewBtn = new QPushButton(i18n("Preview"), this);
  inputLayout->addWidget(m_previewBtn);
  layout->addLayout(inputLayout);

  m_previewLabel = new QLabel(this);
  m_previewLabel->setWordWrap(true);
  layout->addWidget(m_previewLabel);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  m_followBtn = buttonBox->addButton(i18n("Follow"), QDialogButtonBox::AcceptRole);
  m_followBtn->setEnabled(false);
  layout->addWidget(buttonBox);

  connect(m_idEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    m_followBtn->setEnabled(!text.trimmed().isEmpty());
    m_previewLabel->clear();
  });

  connect(m_previewBtn, &QPushButton::clicked, this, &FollowSessionDialog::previewSession);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &FollowSessionDialog::followSession);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString FollowSessionDialog::extractSessionId(const QString &input) const {
  QString text = input.trimmed();
  QRegularExpression re(QStringLiteral("/session/([0-9]+)"));
  QRegularExpressionMatch match = re.match(text);
  if (match.hasMatch()) {
    return match.captured(1);
  }

  // If no URL match, assume it's just the ID
  return text.split(QLatin1Char('/')).last();
}

void FollowSessionDialog::previewSession() {
  QString id = extractSessionId(m_idEdit->text());
  if (id.isEmpty()) {
    QMessageBox::warning(this, i18n("Invalid Input"), i18n("Please enter a valid session ID or URL."));
    return;
  }

  m_previewBtn->setEnabled(false);
  m_previewLabel->setText(i18n("Loading..."));

  // Disconnect any existing connections to prevent multiple triggers
  disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
  disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

  connect(m_apiManager, &APIManager::sessionDetailsReceived, this, [this, id](const QJsonObject &data) {
    disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
    disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

    m_previewBtn->setEnabled(true);
    if (data.isEmpty() || data.value(QStringLiteral("id")).toString() != id) {
      m_previewLabel->setText(i18n("Failed to load session details or ID mismatch."));
      return;
    }

    QString title = data.value(QStringLiteral("title")).toString();
    if (title.isEmpty()) {
      title = data.value(QStringLiteral("prompt")).toString();
    }
    QString status = data.value(QStringLiteral("state")).toString();

    m_previewLabel->setText(i18n("<b>ID:</b> %1<br><b>Title:</b> %2<br><b>Status:</b> %3", id, title, status));
  });

  connect(m_apiManager, &APIManager::errorOccurred, this, [this](const QString &error) {
    disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
    disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

    m_previewBtn->setEnabled(true);
    m_previewLabel->setText(i18n("Error: %1", error));
  });

  m_apiManager->getSession(id);
}

void FollowSessionDialog::followSession() {
  QString id = extractSessionId(m_idEdit->text());
  if (id.isEmpty()) {
    QMessageBox::warning(this, i18n("Invalid Input"), i18n("Please enter a valid session ID or URL."));
    return;
  }

  m_followBtn->setEnabled(false);

  // Disconnect any existing connections
  disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
  disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

  connect(m_apiManager, &APIManager::sessionDetailsReceived, this, [this, id](const QJsonObject &data) {
    disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
    disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

    if (!data.isEmpty() && data.value(QStringLiteral("id")).toString() == id) {
      m_sessionModel->addSession(data);
      m_sessionModel->saveSessions();
      accept();
    } else {
      m_followBtn->setEnabled(true);
      QMessageBox::warning(this, i18n("Error"), i18n("Failed to load session details or ID mismatch."));
    }
  });

  connect(m_apiManager, &APIManager::errorOccurred, this, [this](const QString &error) {
    disconnect(m_apiManager, &APIManager::sessionDetailsReceived, this, nullptr);
    disconnect(m_apiManager, &APIManager::errorOccurred, this, nullptr);

    m_followBtn->setEnabled(true);
    QMessageBox::warning(this, i18n("Error"), i18n("Error fetching session: %1", error));
  });

  m_apiManager->getSession(id);
}
