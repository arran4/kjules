#include "sessionwindow.h"

#include "activitybrowser.h"
#include "apimanager.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <QAction>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

SessionWindow::SessionWindow(const QJsonObject &sessionData,
                             APIManager *apiManager, bool isManaged,
                             QWidget *parent)
    : KXmlGuiWindow(parent), m_sessionData(sessionData),
      m_apiManager(apiManager), m_isManaged(isManaged) {
  setObjectName(QStringLiteral("SessionWindow_%1")
                    .arg(sessionData.value(QStringLiteral("id")).toString()));
  setAttribute(Qt::WA_DeleteOnClose);

  m_autoRefreshTimer = new QTimer(this);
  connect(m_autoRefreshTimer, &QTimer::timeout, this,
          &SessionWindow::refreshSession);

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionReloaded, this,
            &SessionWindow::onSessionReloaded);
    connect(m_apiManager, &APIManager::activitiesReceived, this,
            &SessionWindow::onActivitiesReceived);
  }

  setupActions();
  setupUi(m_sessionData);
  setupGUI();

  KConfigGroup config(KSharedConfig::openConfig(),
                      QStringLiteral("SessionWindow"));
  int autoRefreshIndex = config.readEntry("AutoRefreshIndex", 0);
  m_autoRefreshCombo->setCurrentIndex(autoRefreshIndex);
  updateAutoRefresh();
}

SessionWindow::~SessionWindow() {
  KConfigGroup config(KSharedConfig::openConfig(),
                      QStringLiteral("SessionWindow"));
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
                                         QKeySequence(Qt::CTRL | Qt::Key_W));
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

  // Setup Menu Bar
  QMenuBar *menuBarWidget = menuBar();

  QMenu *sessionMenu = menuBarWidget->addMenu(i18n("&Session"));
  sessionMenu->addAction(refreshAction);
  sessionMenu->addAction(duplicateAction);

  QAction *saveTemplateAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                  i18n("Save prompt as template"), this);
  connect(saveTemplateAction, &QAction::triggered, this, [this]() {
    QJsonObject templateData;
    templateData[QStringLiteral("prompt")] =
        m_sessionData.value(QStringLiteral("prompt")).toString();
    templateData[QStringLiteral("automationMode")] =
        m_sessionData.value(QStringLiteral("automationMode")).toString();
    Q_EMIT templateRequested(templateData);
  });
  sessionMenu->addAction(saveTemplateAction);
  sessionMenu->addSeparator();

  QAction *watchAction =
      new QAction(QIcon::fromTheme(QStringLiteral("visibility")),
                  i18n("Watch Session"), this);
  connect(watchAction, &QAction::triggered, this,
          [this, watchAction, sessionMenu]() {
            Q_EMIT watchRequested(m_sessionData);
            m_isManaged = true;
            watchAction->setEnabled(false);
          });
  if (!m_isManaged) {
    sessionMenu->addAction(watchAction);
  }

  QAction *archiveAction = new QAction(
      QIcon::fromTheme(QStringLiteral("archive")), i18n("Archive"), this);
  connect(archiveAction, &QAction::triggered, this, [this]() {
    Q_EMIT archiveRequested(
        m_sessionData.value(QStringLiteral("id")).toString());
  });
  if (m_isManaged) {
    sessionMenu->addAction(archiveAction);
  }

  QAction *deleteAction = new QAction(
      QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"), this);
  connect(deleteAction, &QAction::triggered, this, [this]() {
    Q_EMIT deleteRequested(
        m_sessionData.value(QStringLiteral("id")).toString());
  });
  if (m_isManaged) {
    sessionMenu->addAction(deleteAction);
  }
  sessionMenu->addSeparator();
  sessionMenu->addAction(closeAction);

  QMenu *linksMenu = menuBarWidget->addMenu(i18n("&Links"));

  QAction *openJulesAction = new QAction(i18n("Open Jules URL"), this);
  connect(openJulesAction, &QAction::triggered, this, [this]() {
    QString id = m_sessionData.value(QStringLiteral("id")).toString();
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://jules.google.com/sessions/") + id));
  });
  linksMenu->addAction(openJulesAction);

  QAction *copyJulesAction = new QAction(i18n("Copy Jules URL"), this);
  connect(copyJulesAction, &QAction::triggered, this, [this]() {
    QString id = m_sessionData.value(QStringLiteral("id")).toString();
    QGuiApplication::clipboard()->setText(
        QStringLiteral("https://jules.google.com/sessions/") + id);
  });
  linksMenu->addAction(copyJulesAction);

  QString prUrlStr;
  QJsonArray outputs = m_sessionData.value(QStringLiteral("outputs")).toArray();
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outObj = outputs[i].toObject();
    if (outObj.contains(QStringLiteral("pullRequest"))) {
      prUrlStr = outObj.value(QStringLiteral("pullRequest"))
                     .toObject()
                     .value(QStringLiteral("url"))
                     .toString();
    }
  }

  if (!prUrlStr.isEmpty()) {
    linksMenu->addSeparator();
    QAction *openPrAction = new QAction(i18n("Open Pull Request URL"), this);
    connect(openPrAction, &QAction::triggered, this,
            [prUrlStr]() { QDesktopServices::openUrl(QUrl(prUrlStr)); });
    linksMenu->addAction(openPrAction);

    QAction *copyPrAction = new QAction(i18n("Copy Pull Request URL"), this);
    connect(copyPrAction, &QAction::triggered, this,
            [prUrlStr]() { QGuiApplication::clipboard()->setText(prUrlStr); });
    linksMenu->addAction(copyPrAction);

    if (m_sessionData.contains(QStringLiteral("githubPrData"))) {
      QJsonObject prData =
          m_sessionData.value(QStringLiteral("githubPrData")).toObject();
      QString authorUrl = prData.value(QStringLiteral("user"))
                              .toObject()
                              .value(QStringLiteral("html_url"))
                              .toString();
      QString branchUrl = prData.value(QStringLiteral("head"))
                              .toObject()
                              .value(QStringLiteral("repo"))
                              .toObject()
                              .value(QStringLiteral("html_url"))
                              .toString() +
                          QStringLiteral("/tree/") +
                          prData.value(QStringLiteral("head"))
                              .toObject()
                              .value(QStringLiteral("ref"))
                              .toString();

      if (!authorUrl.isEmpty()) {
        QAction *copyAuthorUrlAction =
            new QAction(i18n("Copy PR Author URL"), this);
        connect(copyAuthorUrlAction, &QAction::triggered, this, [authorUrl]() {
          QGuiApplication::clipboard()->setText(authorUrl);
        });
        linksMenu->addAction(copyAuthorUrlAction);
      }

      if (!branchUrl.isEmpty()) {
        QAction *copyBranchUrlAction =
            new QAction(i18n("Copy PR Branch URL"), this);
        connect(copyBranchUrlAction, &QAction::triggered, this, [branchUrl]() {
          QGuiApplication::clipboard()->setText(branchUrl);
        });
        linksMenu->addAction(copyBranchUrlAction);
      }
    }
  }

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
    m_sessionData[QStringLiteral("lastRefreshed")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    renderDetailsAndDiff();

    if (m_apiManager) {
      m_apiManager->listActivities(currentId);
    }
  }
}

void SessionWindow::onActivitiesReceived(const QString &sessionId,
                                         const QJsonArray &activities) {
  QString currentId = m_sessionData.value(QStringLiteral("id")).toString();
  if (currentId != sessionId)
    return;

  QJsonArray turns = activities;
  if (turns.isEmpty()) {
    if (m_sessionData.contains(QStringLiteral("turns"))) {
      turns = m_sessionData.value(QStringLiteral("turns")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("history"))) {
      turns = m_sessionData.value(QStringLiteral("history")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("messages"))) {
      turns = m_sessionData.value(QStringLiteral("messages")).toArray();
    } else if (m_sessionData.contains(QStringLiteral("actions"))) {
      turns = m_sessionData.value(QStringLiteral("actions")).toArray();
    }
  }

  QString prompt = m_sessionData.value(QStringLiteral("prompt")).toString();
  m_activityBrowser->setPrompt(prompt);
  m_activityBrowser->setActivities(turns);

  QJsonDocument activitiesDoc(turns);
  m_rawActivitiesBrowser->setPlainText(
      QString::fromUtf8(activitiesDoc.toJson(QJsonDocument::Indented)));

  m_statusLabel->setText(
      i18n("Refreshed at %1",
           QDateTime::currentDateTime().toString(
               QLocale::system().dateFormat(QLocale::ShortFormat))));
}

void SessionWindow::renderDetailsAndDiff() {
  QJsonDocument doc(m_sessionData);
  QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
  m_textBrowser->setPlainText(jsonString);

  QString title = m_sessionData.value(QStringLiteral("title")).toString();
  QString sessionId = m_sessionData.value(QStringLiteral("id")).toString();
  QString lastRefreshed =
      m_sessionData.value(QStringLiteral("lastRefreshed")).toString();
  QString state = m_sessionData.value(QStringLiteral("state")).toString();
  QJsonObject sourceContext =
      m_sessionData.value(QStringLiteral("sourceContext")).toObject();
  QString source = sourceContext.value(QStringLiteral("source")).toString();
  bool environmentVariablesEnabled =
      sourceContext.value(QStringLiteral("environmentVariablesEnabled"))
          .toBool();
  QString startingBranch =
      sourceContext.value(QStringLiteral("githubRepoContext"))
          .toObject()
          .value(QStringLiteral("startingBranch"))
          .toString();
  QString createTime =
      m_sessionData.value(QStringLiteral("createTime")).toString();
  QString updateTime =
      m_sessionData.value(QStringLiteral("updateTime")).toString();
  QString promptText = m_sessionData.value(QStringLiteral("prompt")).toString();

  if (!createTime.isEmpty()) {
    QDateTime dt = QDateTime::fromString(createTime, Qt::ISODate);
    if (dt.isValid()) {
      createTime = dt.toLocalTime().toString(
          QLocale::system().dateFormat(QLocale::ShortFormat));
    }
  }
  if (!updateTime.isEmpty()) {
    QDateTime dt = QDateTime::fromString(updateTime, Qt::ISODate);
    if (dt.isValid()) {
      updateTime = dt.toLocalTime().toString(
          QLocale::system().dateFormat(QLocale::ShortFormat));
    }
  }

  QString detailsHtml =
      QStringLiteral("<html><head><style>") +
      QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                     "line-height: 1.6; }") +
      QStringLiteral(
          "th { text-align: left; padding-right: 15px; color: #555; }") +
      QStringLiteral("a { color: #3498db; text-decoration: none; }") +
      QStringLiteral("a:hover { text-decoration: underline; }") +
      QStringLiteral("</style></head><body><h2>") + i18n("Session Details") +
      QStringLiteral("</h2><table>");

  QString julesUrl =
      QStringLiteral("https://jules.google.com/sessions/") + sessionId;
  detailsHtml += QStringLiteral("<tr><th>") + i18n("ID:") +
                 QStringLiteral("</th><td>") + sessionId.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Jules URL:") +
                 QStringLiteral("</th><td><a href=\"") +
                 julesUrl.toHtmlEscaped() + QStringLiteral("\">") +
                 julesUrl.toHtmlEscaped() + QStringLiteral("</a></td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("State:") +
                 QStringLiteral("</th><td>") + state.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Source:") +
                 QStringLiteral("</th><td>") + source.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  if (!startingBranch.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Starting Branch:") +
                   QStringLiteral("</th><td>") +
                   startingBranch.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
  }
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Env Vars Enabled:") +
                 QStringLiteral("</th><td>") +
                 (environmentVariablesEnabled ? i18n("Yes") : i18n("No")) +
                 QStringLiteral("</td></tr>");
  if (!createTime.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Create Time:") +
                   QStringLiteral("</th><td>") + createTime.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
  }
  if (!updateTime.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Update Time:") +
                   QStringLiteral("</th><td>") + updateTime.toHtmlEscaped() +
                   QStringLiteral("</td></tr>");
  }
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Last Refreshed:") +
                 QStringLiteral("</th><td>") +
                 (lastRefreshed.isEmpty() ? i18n("Never") : lastRefreshed)
                     .toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("</table>");

  QJsonArray outputs = m_sessionData.value(QStringLiteral("outputs")).toArray();
  QString diffText;
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outObj = outputs[i].toObject();
    if (outObj.contains(QStringLiteral("pullRequest"))) {
      QJsonObject prObj =
          outObj.value(QStringLiteral("pullRequest")).toObject();
      QString prUrl = prObj.value(QStringLiteral("url")).toString();
      QString prTitle = prObj.value(QStringLiteral("title")).toString();
      detailsHtml += QStringLiteral("<hr/><h3>") + i18n("Pull Request") +
                     QStringLiteral("</h3><table>");
      detailsHtml += QStringLiteral("<tr><th>") + i18n("Title:") +
                     QStringLiteral("</th><td>") + prTitle.toHtmlEscaped() +
                     QStringLiteral("</td></tr>");
      detailsHtml += QStringLiteral("<tr><th>") + i18n("URL:") +
                     QStringLiteral("</th><td><a href=\"") +
                     prUrl.toHtmlEscaped() + QStringLiteral("\">") +
                     prUrl.toHtmlEscaped() + QStringLiteral("</a></td></tr>");
      detailsHtml += QStringLiteral("</table>");
    }
    if (outObj.contains(QStringLiteral("changeSet"))) {
      QJsonObject changeSet =
          outObj.value(QStringLiteral("changeSet")).toObject();
      if (changeSet.contains(QStringLiteral("gitPatch"))) {
        QJsonObject gitPatch =
            changeSet.value(QStringLiteral("gitPatch")).toObject();
        diffText = gitPatch.value(QStringLiteral("unidiffPatch")).toString();
      }
    }
  }

  detailsHtml += QStringLiteral("</body></html>");

  m_detailsBrowser->setHtml(detailsHtml);

  if (m_githubPrBrowser) {
    if (m_sessionData.contains(QStringLiteral("githubPrData"))) {
      QJsonObject prData =
          m_sessionData.value(QStringLiteral("githubPrData")).toObject();
      QString html =
          QStringLiteral("<html><head><style>") +
          QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                         "line-height: 1.6; }") +
          QStringLiteral(
              "th { text-align: left; padding-right: 15px; color: #555; }") +
          QStringLiteral("a { color: #3498db; text-decoration: none; }") +
          QStringLiteral("a:hover { text-decoration: underline; }") +
          QStringLiteral("</style></head><body><h2>") +
          i18n("Pull Request Summary") + QStringLiteral("</h2><table>");

      html += QStringLiteral("<tr><th>") + i18n("Title:") +
              QStringLiteral("</th><td>") +
              prData.value(QStringLiteral("title")).toString().toHtmlEscaped() +
              QStringLiteral("</td></tr>");
      html += QStringLiteral("<tr><th>") + i18n("State:") +
              QStringLiteral("</th><td>") +
              prData.value(QStringLiteral("state")).toString().toHtmlEscaped() +
              QStringLiteral("</td></tr>");

      QJsonObject userObj = prData.value(QStringLiteral("user")).toObject();
      html +=
          QStringLiteral("<tr><th>") + i18n("Author:") +
          QStringLiteral("</th><td><a href=\"") +
          userObj.value(QStringLiteral("html_url")).toString().toHtmlEscaped() +
          QStringLiteral("\">") +
          userObj.value(QStringLiteral("login")).toString().toHtmlEscaped() +
          QStringLiteral("</a></td></tr>");

      QString branchUrl = prData.value(QStringLiteral("head"))
                              .toObject()
                              .value(QStringLiteral("repo"))
                              .toObject()
                              .value(QStringLiteral("html_url"))
                              .toString() +
                          QStringLiteral("/tree/") +
                          prData.value(QStringLiteral("head"))
                              .toObject()
                              .value(QStringLiteral("ref"))
                              .toString();
      html += QStringLiteral("<tr><th>") + i18n("Branch:") +
              QStringLiteral("</th><td><a href=\"") +
              branchUrl.toHtmlEscaped() + QStringLiteral("\">") +
              prData.value(QStringLiteral("head"))
                  .toObject()
                  .value(QStringLiteral("ref"))
                  .toString()
                  .toHtmlEscaped() +
              QStringLiteral("</a></td></tr>");

      QJsonArray labels = prData.value(QStringLiteral("labels")).toArray();
      QStringList labelStrs;
      for (int i = 0; i < labels.size(); ++i) {
        labelStrs << labels[i]
                         .toObject()
                         .value(QStringLiteral("name"))
                         .toString()
                         .toHtmlEscaped();
      }
      html += QStringLiteral("<tr><th>") + i18n("Labels:") +
              QStringLiteral("</th><td>") +
              labelStrs.join(QStringLiteral(", ")) +
              QStringLiteral("</td></tr>");

      html += QStringLiteral("</table><hr/><h3>") + i18n("Body") +
              QStringLiteral("</h3><pre style=\"white-space: pre-wrap;\">") +
              prData.value(QStringLiteral("body")).toString().toHtmlEscaped() +
              QStringLiteral("</pre></body></html>");

      m_githubPrBrowser->setHtml(html);
    } else {
      m_githubPrBrowser->setPlainText(i18n("No GitHub PR data available."));
    }
  }

  if (m_promptBrowser) {
    m_promptBrowser->setMarkdown(promptText);
  }

  if (m_diffBrowser) {
    if (diffText.isEmpty()) {
      m_diffBrowser->setPlainText(i18n("No diff available."));
    } else {
      m_diffBrowser->setPlainText(diffText);
    }
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
  m_detailsBrowser->setOpenExternalLinks(true);

  m_promptBrowser = new QTextBrowser(this);

  m_diffBrowser = new QTextBrowser(this);
  m_githubPrBrowser = new QTextBrowser(this);
  m_githubPrBrowser->setOpenExternalLinks(true);
  m_diffBrowser->setStyleSheet(QStringLiteral("font-family: monospace;"));

  m_rawActivitiesBrowser = new QTextBrowser(this);
  m_activityBrowser = new ActivityBrowser(this);
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
  m_tabWidget->addTab(m_promptBrowser, i18n("Prompt"));
  m_tabWidget->addTab(m_diffBrowser, i18n("Diff"));
  m_tabWidget->addTab(m_githubPrBrowser, i18n("GitHub PR Summary"));
  m_tabWidget->addTab(m_activityTabWidget, i18n("Activity Feed"));
  m_tabWidget->addTab(m_rawActivitiesBrowser, i18n("Raw Activities"));
  m_tabWidget->addTab(m_textBrowser, i18n("Raw JSON"));

  QString title = sessionData.value(QStringLiteral("title")).toString();
  QString sessionId = sessionData.value(QStringLiteral("id")).toString();
  if (title.isEmpty()) {
    title = i18n("Details");
  }
  setWindowTitle(i18n("Session %1 - %2", sessionId, title));

  renderDetailsAndDiff();

  // Load activities initially if API manager exists, otherwise fallback to
  // embedded in onActivitiesReceived
  if (m_apiManager) {
    if (m_statusLabel)
      m_statusLabel->setText(i18n("Loading activities..."));
    m_apiManager->listActivities(sessionId);
  } else {
    onActivitiesReceived(sessionId, QJsonArray());
  }

  resize(800, 600);
}
