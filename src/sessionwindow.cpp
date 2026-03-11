#include "sessionwindow.h"

#include "apimanager.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

SessionWindow::SessionWindow(const QJsonObject &sessionData,
                             APIManager *apiManager, QWidget *parent)
    : KXmlGuiWindow(parent), m_sessionData(sessionData),
      m_apiManager(apiManager) {
  setAttribute(Qt::WA_DeleteOnClose);

  m_autoRefreshTimer = new QTimer(this);
  connect(m_autoRefreshTimer, &QTimer::timeout, this,
          &SessionWindow::refreshSession);

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionReloaded, this,
            &SessionWindow::onSessionReloaded);
  }

  setupUi(m_sessionData);
  setupActions();

  KConfigGroup config(KSharedConfig::openConfig(), "SessionWindow");
  int autoRefreshIndex = config.readEntry("AutoRefreshIndex", 0);
  m_autoRefreshCombo->setCurrentIndex(autoRefreshIndex);
  updateAutoRefresh();
}

SessionWindow::~SessionWindow() {
  KConfigGroup config(KSharedConfig::openConfig(), "SessionWindow");
  config.writeEntry("AutoRefreshIndex", m_autoRefreshCombo->currentIndex());
  config.sync();
}

void SessionWindow::setupActions() {
  QAction *refreshAction = new QAction(
      QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
  actionCollection()->addAction(QStringLiteral("refresh_session"),
                                refreshAction);
  actionCollection()->setDefaultShortcut(refreshAction,
                                         QKeySequence(Qt::Key_F5));
  connect(refreshAction, &QAction::triggered, this,
          &SessionWindow::refreshSession);

  QAction *duplicateAction =
      new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                  i18n("Duplicate Session"), this);
  actionCollection()->addAction(QStringLiteral("duplicate_session"),
                                duplicateAction);
  connect(duplicateAction, &QAction::triggered, this,
          &SessionWindow::duplicateSession);

  QAction *closeAction = new QAction(
      QIcon::fromTheme(QStringLiteral("window-close")), i18n("Close"), this);
  actionCollection()->addAction(QStringLiteral("close_window"), closeAction);
  actionCollection()->setDefaultShortcut(closeAction,
                                         QKeySequence(Qt::CTRL + Qt::Key_W));
  connect(closeAction, &QAction::triggered, this, &SessionWindow::close);

  setStandardToolBarMenuEnabled(true);

  KToolBar *toolBar = new KToolBar(QStringLiteral("mainToolBar"), this);
  toolBar->addAction(refreshAction);
  toolBar->addAction(duplicateAction);
  toolBar->addSeparator();

  toolBar->addWidget(new QLabel(i18n(" Auto Refresh: "), this));
  m_autoRefreshCombo = new QComboBox(this);
  m_autoRefreshCombo->addItem(i18n("Off"), 0);
  m_autoRefreshCombo->addItem(i18n("10 seconds"), 10);
  m_autoRefreshCombo->addItem(i18n("30 seconds"), 30);
  m_autoRefreshCombo->addItem(i18n("1 minute"), 60);
  m_autoRefreshCombo->addItem(i18n("5 minutes"), 300);
  connect(m_autoRefreshCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SessionWindow::updateAutoRefresh);
  toolBar->addWidget(m_autoRefreshCombo);

  toolBar->addSeparator();
  toolBar->addAction(closeAction);

  addToolBar(toolBar);

  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);
}

void SessionWindow::updateAutoRefresh() {
  int seconds = m_autoRefreshCombo->currentData().toInt();
  if (seconds > 0) {
    m_autoRefreshTimer->start(seconds * 1000);
  } else {
    m_autoRefreshTimer->stop();
  }
}

void SessionWindow::refreshSession() {
  if (m_apiManager) {
    QString id = m_sessionData.value(QStringLiteral("id")).toString();
    m_apiManager->reloadSession(id);
    m_statusLabel->setText(i18n("Refreshing..."));
  }
}

void SessionWindow::onSessionReloaded(const QJsonObject &session) {
  QString currentId = m_sessionData.value(QStringLiteral("id")).toString();
  QString incomingId = session.value(QStringLiteral("id")).toString();

  if (currentId == incomingId) {
    m_sessionData = session;

    // Re-generate tabs content instead of calling setupUi again (which would
    // leak widgets/tabs)
    QJsonDocument doc(m_sessionData);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    m_textBrowser->setPlainText(jsonString);

    QString title = m_sessionData.value(QStringLiteral("title")).toString();
    QString sessionId = m_sessionData.value(QStringLiteral("id")).toString();
    QString lastRefreshed =
        m_sessionData.value(QStringLiteral("lastRefreshed")).toString();
    QString state = m_sessionData.value(QStringLiteral("state")).toString();
    QString source = m_sessionData.value(QStringLiteral("sourceContext"))
                         .toObject()
                         .value(QStringLiteral("source"))
                         .toString();
    QString promptText =
        m_sessionData.value(QStringLiteral("prompt")).toString();

    QString detailsHtml =
        QStringLiteral("<html><head><style>") +
        QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                       "line-height: 1.6; }") +
        QStringLiteral(
            "th { text-align: left; padding-right: 15px; color: #555; }") +
        QStringLiteral("</style></head><body><h2>") + i18n("Session Details") +
        QStringLiteral("</h2><table>");

    detailsHtml += QStringLiteral("<tr><th>") + i18n("Title:") +
                   QStringLiteral("</th><td>") + title.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
    detailsHtml += QStringLiteral("<tr><th>") + i18n("ID:") +
                   QStringLiteral("</th><td>") + sessionId.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
    detailsHtml += QStringLiteral("<tr><th>") + i18n("State:") +
                   QStringLiteral("</th><td>") + state.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Source:") +
                   QStringLiteral("</th><td>") + source.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Last Refreshed:") +
                   QStringLiteral("</th><td>") +
                   (lastRefreshed.isEmpty() ? i18n("Never") : lastRefreshed)
                       .toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
    detailsHtml += QStringLiteral("</table><hr/><h3>") + i18n("Prompt") +
                   QStringLiteral("</h3><pre>") + promptText.toHtmlEscaped() +
                   QStringLiteral("</pre></body></html>");

    m_detailsBrowser->setHtml(detailsHtml);

    // Just run the same logic for html activity feed without setting up ui
    QString html =
        QStringLiteral("<html><head><style>") +
        QStringLiteral("body { font-family: sans-serif; }") +
        QStringLiteral(".prompt { font-weight: bold; font-size: 1.1em; color: "
                       "#2c3e50; margin-bottom: 10px; }") +
        QStringLiteral(".turn { margin-bottom: 15px; padding: 10px; "
                       "border-radius: 5px; }") +
        QStringLiteral(".user { background-color: #e8f4f8; border-left: 4px "
                       "solid #3498db; }") +
        QStringLiteral(".system { background-color: #f9f9f9; border-left: 4px "
                       "solid #95a5a6; }") +
        QStringLiteral(".assistant { background-color: #eafaf1; border-left: "
                       "4px solid #2ecc71; }") +
        QStringLiteral(
            ".role { font-weight: bold; text-transform: capitalize; "
            "margin-bottom: 5px; color: #7f8c8d; font-size: 0.9em; }") +
        QStringLiteral(".content { white-space: pre-wrap; }") +
        QStringLiteral("</style></head><body>");

    QString prompt = m_sessionData.value(QStringLiteral("prompt")).toString();
    if (!prompt.isEmpty()) {
      html += QStringLiteral("<div class='prompt'>") + i18n("Prompt:") +
              QStringLiteral(" ") + prompt.toHtmlEscaped() +
              QStringLiteral("</div><hr>");
    }

    QJsonArray turns;
    if (m_sessionData.contains(QStringLiteral("turns"))) {
      turns = m_sessionData.value(QStringLiteral("turns")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("history"))) {
      turns = m_sessionData.value(QStringLiteral("history")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("messages"))) {
      turns = m_sessionData.value(QStringLiteral("messages")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("actions"))) {
      turns = m_sessionData.value(QStringLiteral("actions")).toArray();
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

    m_statusLabel->setText(i18n(
        "Refreshed at %1",
        QDateTime::currentDateTime().toString(Qt::DefaultLocaleShortDate)));
  }
}

void SessionWindow::duplicateSession() {
  QString prompt = m_sessionData.value(QStringLiteral("prompt")).toString();
  QString source = m_sessionData.value(QStringLiteral("sourceContext"))
                       .toObject()
                       .value(QStringLiteral("source"))
                       .toString();

  QJsonObject req;
  req[QStringLiteral("source")] = source;
  req[QStringLiteral("prompt")] = prompt;

  if (m_apiManager) {
    m_apiManager->createSessionAsync(req);
    m_statusLabel->setText(i18n("Duplicate session requested."));
  }
}

void SessionWindow::setupUi(const QJsonObject &sessionData) {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_tabWidget = new QTabWidget(this);
  mainLayout->addWidget(m_tabWidget);

  m_detailsBrowser = new QTextBrowser(this);
  m_activityBrowser = new QTextBrowser(this);
  m_textBrowser = new QTextBrowser(this);

  m_activityTabWidget = new QWidget(this);
  QVBoxLayout *activityLayout = new QVBoxLayout(m_activityTabWidget);
  activityLayout->addWidget(m_activityBrowser);

  QHBoxLayout *chatInputLayout = new QHBoxLayout();
  m_chatInput = new QLineEdit(this);
  m_chatInput->setPlaceholderText(i18n("Type a message..."));
  QPushButton *sendButton = new QPushButton(
      QIcon::fromTheme(QStringLiteral("mail-send")), i18n("Send"), this);
  chatInputLayout->addWidget(m_chatInput);
  chatInputLayout->addWidget(sendButton);
  activityLayout->addLayout(chatInputLayout);

  connect(sendButton, &QPushButton::clicked, this, [this]() {
    QString text = m_chatInput->text().trimmed();
    if (text.isEmpty())
      return;
    m_chatInput->clear();
    if (m_statusLabel) {
      m_statusLabel->setText(
          i18n("Sending message not yet implemented by Jules API..."));
    }
  });
  connect(m_chatInput, &QLineEdit::returnPressed, sendButton,
          &QPushButton::click);

  m_tabWidget->addTab(m_detailsBrowser, i18n("Details"));
  m_tabWidget->addTab(m_activityTabWidget, i18n("Activity Feed"));
  m_tabWidget->addTab(m_textBrowser, i18n("Raw JSON"));

  QString title = sessionData.value(QStringLiteral("title")).toString();
  QString sessionId = sessionData.value(QStringLiteral("id")).toString();
  if (title.isEmpty()) {
    title = i18n("Session Details");
  }
  setWindowTitle(i18n("%1 - %2", title, sessionId));

  QJsonDocument doc(sessionData);
  QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
  m_textBrowser->setPlainText(jsonString);

  QString lastRefreshed =
      sessionData.value(QStringLiteral("lastRefreshed")).toString();
  QString state = sessionData.value(QStringLiteral("state")).toString();
  QString source = sessionData.value(QStringLiteral("sourceContext"))
                       .toObject()
                       .value(QStringLiteral("source"))
                       .toString();
  QString promptText = sessionData.value(QStringLiteral("prompt")).toString();

  QString detailsHtml =
      QStringLiteral("<html><head><style>") +
      QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                     "line-height: 1.6; }") +
      QStringLiteral(
          "th { text-align: left; padding-right: 15px; color: #555; }") +
      QStringLiteral("</style></head><body><h2>") + i18n("Session Details") +
      QStringLiteral("</h2><table>");

  detailsHtml += QStringLiteral("<tr><th>") + i18n("Title:") +
                 QStringLiteral("</th><td>") + title.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("ID:") +
                 QStringLiteral("</th><td>") + sessionId.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("State:") +
                 QStringLiteral("</th><td>") + state.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Source:") +
                 QStringLiteral("</th><td>") + source.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Last Refreshed:") +
                 QStringLiteral("</th><td>") +
                 (lastRefreshed.isEmpty() ? i18n("Never") : lastRefreshed)
                     .toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("</table><hr/><h3>") + i18n("Prompt") +
                 QStringLiteral("</h3><pre>") + promptText.toHtmlEscaped() +
                 QStringLiteral("</pre></body></html>");

  m_detailsBrowser->setHtml(detailsHtml);

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
  } else if (sessionData.contains(QStringLiteral("actions"))) {
    turns = sessionData.value(QStringLiteral("actions")).toArray();
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
