#include "sessionwindow.h"

#include <KLocalizedString>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

SessionWindow::SessionWindow(const QJsonObject &sessionData, QWidget *parent)
    : KXmlGuiWindow(parent) {
  setAttribute(Qt::WA_DeleteOnClose);
  setupUi(sessionData);
}

SessionWindow::~SessionWindow() {}

void SessionWindow::setupUi(const QJsonObject &sessionData) {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);
  mainLayout->addWidget(m_tabWidget);

  m_activityBrowser = new QTextBrowser(this);
  m_textBrowser = new QTextBrowser(this);

  m_tabWidget->addTab(m_activityBrowser, i18n("Activity Feed"));
  m_tabWidget->addTab(m_textBrowser, i18n("Raw JSON"));

  QString title = sessionData.value(QStringLiteral("title")).toString();
  setWindowTitle(title.isEmpty() ? QStringLiteral("Session Details") : title);

  QJsonDocument doc(sessionData);
  QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
  m_textBrowser->setPlainText(jsonString);

  // Parse activity feed
  QString html =
      QStringLiteral("<html><head><style>") +
      QStringLiteral("body { font-family: sans-serif; }") +
      QStringLiteral(".prompt { font-weight: bold; font-size: 1.1em; color: "
                     "#2c3e50; margin-bottom: 10px; }") +
      QStringLiteral(
          ".turn { margin-bottom: 15px; padding: 10px; border-radius: 5px; }") +
      QStringLiteral(".user { background-color: #e8f4f8; border-left: 4px "
                     "solid #3498db; }") +
      QStringLiteral(".system { background-color: #f9f9f9; border-left: 4px "
                     "solid #95a5a6; }") +
      QStringLiteral(".assistant { background-color: #eafaf1; border-left: 4px "
                     "solid #2ecc71; }") +
      QStringLiteral(
          ".role { font-weight: bold; text-transform: capitalize; "
          "margin-bottom: 5px; color: #7f8c8d; font-size: 0.9em; }") +
      QStringLiteral(".content { white-space: pre-wrap; }") +
      QStringLiteral("</style></head><body>");

  QString prompt = sessionData.value(QStringLiteral("prompt")).toString();
  if (!prompt.isEmpty()) {
    html += QStringLiteral("<div class='prompt'>") + i18n("Prompt:") +
            QStringLiteral(" ") + prompt.toHtmlEscaped() +
            QStringLiteral("</div><hr>");
  }

  QJsonArray turns;
  if (sessionData.contains(QStringLiteral("turns"))) {
    turns = sessionData.value(QStringLiteral("turns")).toArray();
  } else if (sessionData.contains(QStringLiteral("history"))) {
    turns = sessionData.value(QStringLiteral("history")).toArray();
  } else if (sessionData.contains(QStringLiteral("messages"))) {
    turns = sessionData.value(QStringLiteral("messages")).toArray();
  }

  if (turns.isEmpty()) {
    html += QStringLiteral("<p><i>") + i18n("No activity feed available.") +
            QStringLiteral("</i></p>");
  } else {
    for (int i = 0; i < turns.size(); ++i) {
      QJsonObject turn = turns[i].toObject();
      QString role = turn.value(QStringLiteral("role")).toString();
      if (role.isEmpty()) {
        role = turn.value(QStringLiteral("author")).toString();
      }
      if (role.isEmpty()) {
        role = QStringLiteral("system");
      }

      QString content = turn.value(QStringLiteral("content")).toString();
      if (content.isEmpty()) {
        content = turn.value(QStringLiteral("text")).toString();
      }
      if (content.isEmpty() && turn.contains(QStringLiteral("parts"))) {
        QJsonArray parts = turn.value(QStringLiteral("parts")).toArray();
        for (int j = 0; j < parts.size(); ++j) {
          content +=
              parts[j].toObject().value(QStringLiteral("text")).toString() +
              QStringLiteral("\n");
        }
      }

      QString roleClass = role.toLower();
      if (roleClass != QStringLiteral("user") &&
          roleClass != QStringLiteral("assistant") &&
          roleClass != QStringLiteral("system")) {
        roleClass = QStringLiteral("system");
      }

      html += QStringLiteral("<div class='turn ") + roleClass +
              QStringLiteral("'>");
      html += QStringLiteral("<div class='role'>") + role.toHtmlEscaped() +
              QStringLiteral("</div>");
      html += QStringLiteral("<div class='content'>") +
              content.toHtmlEscaped() + QStringLiteral("</div>");
      html += QStringLiteral("</div>");
    }
  }

  html += QStringLiteral("</body></html>");
  m_activityBrowser->setHtml(html);

  resize(800, 600);
}
