#include "followsessiondialog.h"
#include "apimanager.h"
#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

FollowSessionDialog::FollowSessionDialog(APIManager *apiManager,
                                         QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager) {
  setWindowTitle(i18n("Follow Session"));

  QVBoxLayout *layout = new QVBoxLayout(this);

  QLabel *infoLabel = new QLabel(i18n("Enter Jules Session ID or URL:"), this);
  layout->addWidget(infoLabel);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  m_inputEdit = new QLineEdit(this);
  m_inputEdit->setPlaceholderText(i18n(
      "e.g. 14074060995680401415 or https://jules.google.com/session/..."));
  inputLayout->addWidget(m_inputEdit);

  m_previewBtn = new QPushButton(i18n("Preview"), this);
  inputLayout->addWidget(m_previewBtn);

  layout->addLayout(inputLayout);

  m_previewLabel = new QLabel(this);
  m_previewLabel->setWordWrap(true);
  m_previewLabel->hide();
  layout->addWidget(m_previewLabel);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_followBtn = buttonBox->button(QDialogButtonBox::Ok);
  m_followBtn->setText(i18n("Follow"));
  m_followBtn->setEnabled(false);

  layout->addWidget(buttonBox);

  connect(m_inputEdit, &QLineEdit::textChanged, this,
          &FollowSessionDialog::updateButtons);
  connect(m_previewBtn, &QPushButton::clicked, this,
          &FollowSessionDialog::onPreviewClicked);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_apiManager, &APIManager::sessionDetailsReceived, this,
          &FollowSessionDialog::onSessionReceived);
  connect(m_apiManager, &APIManager::errorOccurred, this,
          &FollowSessionDialog::onErrorOccurred);
}

QString FollowSessionDialog::extractSessionId(const QString &input) const {
  QString text = input.trimmed();
  if (text.isEmpty()) {
    return QString();
  }

  while (text.endsWith(QLatin1Char('/'))) {
    text.chop(1);
  }

  QUrl url(text);
  if (url.isValid() && !url.scheme().isEmpty()) {
    QString path = url.path();
    if (path.startsWith(QStringLiteral("/sessions/"))) {
      return path.mid(10);
    }
  }

  // If it's just a number or string
  int lastSlash = text.lastIndexOf(QLatin1Char('/'));
  if (lastSlash != -1) {
    return text.mid(lastSlash + 1);
  }

  return text;
}

QString FollowSessionDialog::sessionId() const {
  return extractSessionId(m_inputEdit->text());
}

void FollowSessionDialog::updateButtons() {
  m_followBtn->setEnabled(!m_inputEdit->text().trimmed().isEmpty());
  m_previewLabel->hide();
}

void FollowSessionDialog::onPreviewClicked() {
  QString id = sessionId();
  if (id.isEmpty()) {
    return;
  }
  m_previewBtn->setEnabled(false);
  m_previewLabel->setText(i18n("Fetching details..."));
  m_previewLabel->show();
  m_apiManager->getSession(id);
}

void FollowSessionDialog::onSessionReceived(const QJsonObject &session) {
  if (!isVisible())
    return;

  QString id = session.value(QStringLiteral("id")).toString();
  if (id != sessionId()) {
    return; // Received for another session
  }

  m_sessionData = session;
  m_previewBtn->setEnabled(true);
  QString title = session.value(QStringLiteral("title")).toString();
  if (title.isEmpty()) {
    title = i18n("No title");
  }
  QString state = session.value(QStringLiteral("state")).toString();
  m_previewLabel->setText(i18n("Found: %1 (State: %2)", title, state));
  m_previewLabel->setStyleSheet(QStringLiteral("color: green;"));
  m_followBtn->setEnabled(true);
}

QJsonObject FollowSessionDialog::sessionData() const { return m_sessionData; }

void FollowSessionDialog::onErrorOccurred(const QString &error) {
  if (!isVisible())
    return;
  m_previewBtn->setEnabled(true);
  m_previewLabel->setText(i18n("Error: %1", error));
  m_previewLabel->setStyleSheet(QStringLiteral("color: red;"));
}
