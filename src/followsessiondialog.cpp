#include "followsessiondialog.h"
#include "apimanager.h"
#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

FollowSessionDialog::FollowSessionDialog(APIManager *apiManager, QWidget *parent)
    : QDialog(parent), m_apiManager(apiManager) {
  setWindowTitle(i18n("Follow Session"));

  QVBoxLayout *layout = new QVBoxLayout(this);

  QLabel *infoLabel = new QLabel(i18n("Enter Jules Session IDs or URLs (one per line or space-separated):"), this);
  layout->addWidget(infoLabel);

  m_inputEdit = new QTextEdit(this);
  m_inputEdit->setAcceptRichText(false);
  m_inputEdit->setPlaceholderText(i18n("e.g. 14074060995680401415 or %1...").arg(APIManager::julesSessionBaseUrl()));
  m_inputEdit->setMinimumHeight(100);
  layout->addWidget(m_inputEdit);

  m_previewBtn = new QPushButton(i18n("Preview"), this);
  layout->addWidget(m_previewBtn);

  m_previewLabel = new QLabel(this);
  m_previewLabel->setWordWrap(true);
  m_previewLabel->hide();
  layout->addWidget(m_previewLabel);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_followBtn = buttonBox->button(QDialogButtonBox::Ok);
  m_followBtn->setText(i18n("Follow"));
  m_followBtn->setEnabled(false);

  layout->addWidget(buttonBox);

  connect(m_inputEdit, &QTextEdit::textChanged, this, &FollowSessionDialog::updateButtons);
  connect(m_previewBtn, &QPushButton::clicked, this, &FollowSessionDialog::onPreviewClicked);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_apiManager, &APIManager::sessionDetailsReceived, this, &FollowSessionDialog::onSessionReceived);
  connect(m_apiManager, &APIManager::errorOccurred, this, &FollowSessionDialog::onErrorOccurred);
}

QStringList FollowSessionDialog::extractSessionIds(const QString &input) const {
  QStringList ids;
  QStringList parts = input.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);

  for (QString text : parts) {
    text = text.trimmed();
    if (text.isEmpty()) {
      continue;
    }

    while (text.endsWith(QLatin1Char('/'))) {
      text.chop(1);
    }

    QUrl url(text);
    if (url.isValid() && !url.scheme().isEmpty()) {
      QString path = url.path();
      if (path.startsWith(QStringLiteral("/session/"))) {
        ids.append(path.mid(9));
        continue;
      } else if (path.startsWith(QStringLiteral("/sessions/"))) {
        ids.append(path.mid(10));
        continue;
      }
    }

    // If it's just a number or string
    int lastSlash = text.lastIndexOf(QLatin1Char('/'));
    if (lastSlash != -1) {
      ids.append(text.mid(lastSlash + 1));
    } else {
      ids.append(text);
    }
  }

  ids.removeDuplicates();
  return ids;
}

QStringList FollowSessionDialog::sessionIds() const { return extractSessionIds(m_inputEdit->toPlainText()); }

void FollowSessionDialog::updateButtons() {
  m_followBtn->setEnabled(!sessionIds().isEmpty());
  m_previewLabel->hide();
}

void FollowSessionDialog::onPreviewClicked() {
  m_previewIds = sessionIds();
  if (m_previewIds.isEmpty()) {
    return;
  }
  m_previewBtn->setEnabled(false);
  m_sessionDataMap.clear();
  m_pendingPreviewCount = m_previewIds.size();

  if (m_previewIds.size() == 1) {
    m_previewLabel->setText(i18n("Fetching details..."));
  } else {
    m_previewLabel->setText(i18n("Fetching details for %1 sessions...", m_previewIds.size()));
  }
  m_previewLabel->show();

  for (const QString &id : m_previewIds) {
    m_apiManager->getSession(id);
  }
}

void FollowSessionDialog::onSessionReceived(const QJsonObject &session) {
  if (!isVisible())
    return;

  QString id = session.value(QStringLiteral("id")).toString();
  if (!m_previewIds.contains(id)) {
    return; // Received for another session
  }

  // Prevent multiple decrements for the same ID if multiple responses somehow arrive
  if (!m_sessionDataMap.contains(id)) {
    m_sessionDataMap.insert(id, session);
    m_pendingPreviewCount--;
  }

  if (m_pendingPreviewCount <= 0) {
    m_previewBtn->setEnabled(true);
    if (m_previewIds.size() == 1) {
      QString title = session.value(QStringLiteral("title")).toString();
      if (title.isEmpty()) {
        title = i18n("No title");
      }
      QString state = session.value(QStringLiteral("state")).toString();
      m_previewLabel->setText(i18n("Found: %1 (State: %2)", title, state));
    } else {
      m_previewLabel->setText(i18n("Found %1 sessions", m_sessionDataMap.size()));
    }
    m_previewLabel->setStyleSheet(QStringLiteral("color: green;"));
    m_followBtn->setEnabled(true);
  } else {
    m_previewLabel->setText(
        i18n("Fetching details... (%1/%2)", m_previewIds.size() - m_pendingPreviewCount, m_previewIds.size()));
  }
}

QMap<QString, QJsonObject> FollowSessionDialog::sessionDataMap() const { return m_sessionDataMap; }

void FollowSessionDialog::onErrorOccurred(const QString &error) {
  if (!isVisible())
    return;

  if (m_pendingPreviewCount > 0) {
    m_pendingPreviewCount--;
  }

  if (m_pendingPreviewCount <= 0) {
    m_previewBtn->setEnabled(true);
    if (m_previewIds.size() == 1) {
      m_previewLabel->setText(i18n("Error: %1", error));
      m_previewLabel->setStyleSheet(QStringLiteral("color: red;"));
    } else {
      m_previewLabel->setText(i18n("Finished with some errors. Found %1 valid sessions.", m_sessionDataMap.size()));
      if (m_sessionDataMap.isEmpty()) {
        m_previewLabel->setStyleSheet(QStringLiteral("color: red;"));
      } else {
        m_previewLabel->setStyleSheet(QStringLiteral("color: orange;"));
      }
    }
  }
}
