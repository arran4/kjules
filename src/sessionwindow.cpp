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
#include <QWidgetAction>

SessionWindow::SessionWindow(const QJsonObject &sessionData,
                             APIManager *apiManager, bool isManaged,
                             QWidget *parent)
    : KXmlGuiWindow(parent), m_sessionData(sessionData),
      m_apiManager(apiManager), m_isManaged(isManaged), m_tabWidget(nullptr),
      m_statusLabel(nullptr), m_autoRefreshTimer(nullptr),
      m_autoRefreshCombo(nullptr), m_detailsBrowser(nullptr),
      m_promptBrowser(nullptr), m_diffBrowser(nullptr),
      m_activityBrowser(nullptr), m_rawActivitiesBrowser(nullptr) {
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

  setupUi(m_sessionData);
  setupActions();

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
  m_autoRefreshCombo = new QComboBox(this);
  m_autoRefreshCombo->addItem(i18n("Off"), 0);
  m_autoRefreshCombo->addItem(i18n("10 seconds"), 10);
  m_autoRefreshCombo->addItem(i18n("30 seconds"), 30);
  m_autoRefreshCombo->addItem(i18n("1 minute"), 60);
  m_autoRefreshCombo->addItem(i18n("5 minutes"), 300);
  connect(m_autoRefreshCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SessionWindow::updateAutoRefresh);
  // Setup Menu Bar

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
  actionCollection()->addAction(QStringLiteral("save_template"),
                                saveTemplateAction);

  QAction *watchAction =
      new QAction(QIcon::fromTheme(QStringLiteral("visibility")),
                  i18n("Follow Session"), this);
  connect(watchAction, &QAction::triggered, this, [this, watchAction]() {
    Q_EMIT watchRequested(m_sessionData);
    m_isManaged = true;
    watchAction->setEnabled(false);
  });
  actionCollection()->addAction(QStringLiteral("watch_session"), watchAction);

  if (m_isManaged)
    watchAction->setEnabled(false);

  QAction *archiveAction =
      new QAction(QIcon::fromTheme(QStringLiteral("archive")),
                  i18n("Archive Session"), this);
  connect(archiveAction, &QAction::triggered, this, [this]() {
    Q_EMIT archiveRequested(
        m_sessionData.value(QStringLiteral("id")).toString());
  });
  actionCollection()->addAction(QStringLiteral("archive_session"),
                                archiveAction);
  if (!m_isManaged)
    archiveAction->setEnabled(false);

  QAction *deleteAction =
      new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                  i18n("Unmanage Session"), this);
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this, [this]() {
    Q_EMIT deleteRequested(
        m_sessionData.value(QStringLiteral("id")).toString());
  });
  actionCollection()->addAction(QStringLiteral("delete_session"), deleteAction);
  if (!m_isManaged)
    deleteAction->setEnabled(false);

  QAction *openJulesAction = new QAction(i18n("Open Jules URL"), this);
  connect(openJulesAction, &QAction::triggered, this, [this]() {
    QString id = m_sessionData.value(QStringLiteral("id")).toString();
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://jules.google.com/sessions/") + id));
  });
  actionCollection()->addAction(QStringLiteral("open_jules"), openJulesAction);

  QAction *copyJulesAction = new QAction(i18n("Copy Jules URL"), this);
  connect(copyJulesAction, &QAction::triggered, this, [this]() {
    QString id = m_sessionData.value(QStringLiteral("id")).toString();
    QGuiApplication::clipboard()->setText(
        QStringLiteral("https://jules.google.com/sessions/") + id);
  });
  actionCollection()->addAction(QStringLiteral("copy_jules"), copyJulesAction);

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
    QAction *openPrAction = new QAction(i18n("Open Pull Request URL"), this);
    connect(openPrAction, &QAction::triggered, this,
            [prUrlStr]() { QDesktopServices::openUrl(QUrl(prUrlStr)); });
    actionCollection()->addAction(QStringLiteral("open_pr"), openPrAction);

    QAction *copyPrAction = new QAction(i18n("Copy Pull Request URL"), this);
    connect(copyPrAction, &QAction::triggered, this,
            [prUrlStr]() { QGuiApplication::clipboard()->setText(prUrlStr); });
    actionCollection()->addAction(QStringLiteral("copy_pr"), copyPrAction);

    if (m_sessionData.contains(QStringLiteral("githubPrInfo"))) {
      QJsonObject prInfo =
          m_sessionData.value(QStringLiteral("githubPrInfo")).toObject();
      if (prInfo.contains(QStringLiteral("head"))) {
        QString branchName = prInfo.value(QStringLiteral("head"))
                                 .toObject()
                                 .value(QStringLiteral("ref"))
                                 .toString();
        QString branchUrl = prInfo.value(QStringLiteral("head"))
                                .toObject()
                                .value(QStringLiteral("repo"))
                                .toObject()
                                .value(QStringLiteral("html_url"))
                                .toString() +
                            QStringLiteral("/tree/") + branchName;

        QAction *openBranchAction = new QAction(i18n("Open Branch URL"), this);
        connect(openBranchAction, &QAction::triggered, this,
                [branchUrl]() { QDesktopServices::openUrl(QUrl(branchUrl)); });
        actionCollection()->addAction(QStringLiteral("open_branch"),
                                      openBranchAction);

        QAction *copyBranchAction = new QAction(i18n("Copy Branch URL"), this);
        connect(copyBranchAction, &QAction::triggered, this, [branchUrl]() {
          QGuiApplication::clipboard()->setText(branchUrl);
        });
        actionCollection()->addAction(QStringLiteral("copy_branch"),
                                      copyBranchAction);
      }
    }
  }

  QWidget *comboContainer = new QWidget(this);
  QHBoxLayout *comboLayout = new QHBoxLayout(comboContainer);
  comboLayout->setContentsMargins(0, 0, 0, 0);
  comboLayout->addWidget(new QLabel(i18n(" Auto Refresh: "), this));
  comboLayout->addWidget(m_autoRefreshCombo);

  QWidgetAction *autoRefreshAction = new QWidgetAction(this);
  autoRefreshAction->setDefaultWidget(comboContainer);
  actionCollection()->addAction(QStringLiteral("auto_refresh_combo"),
                                autoRefreshAction);

  setupGUI(Default, QStringLiteral("sessionwindowui.rc"));

  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);

  if (m_apiManager) {
    m_statusLabel->setText(i18n("Loading activities..."));
  }
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

  if (m_sessionData.contains(QStringLiteral("githubPrInfo"))) {
    QJsonObject prInfo =
        m_sessionData.value(QStringLiteral("githubPrInfo")).toObject();
    QString prHtml =
        QStringLiteral("<html><head><style>") +
        QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                       "line-height: 1.6; }") +
        QStringLiteral(
            "th { text-align: left; padding-right: 15px; color: #555; }") +
        QStringLiteral("a { color: #3498db; text-decoration: none; }") +
        QStringLiteral("a:hover { text-decoration: underline; }") +
        QStringLiteral("</style></head><body><h2>") +
        i18n("Pull Request Summary") + QStringLiteral("</h2><table>");

    prHtml += QStringLiteral("<tr><th>") + i18n("Title:") +
              QStringLiteral("</th><td>") +
              prInfo.value(QStringLiteral("title")).toString().toHtmlEscaped() +
              QStringLiteral("</td></tr>");
    QString state = prInfo.value(QStringLiteral("state")).toString();
    if (prInfo.value(QStringLiteral("merged_at")).isString()) {
      state = QStringLiteral("merged");
    }
    prHtml += QStringLiteral("<tr><th>") + i18n("State:") +
              QStringLiteral("</th><td>") + state.toHtmlEscaped() +
              QStringLiteral("</td></tr>");

    QJsonArray labels = prInfo.value(QStringLiteral("labels")).toArray();
    if (!labels.isEmpty()) {
      QStringList labelNames;
      for (int i = 0; i < labels.size(); ++i) {
        labelNames.append(
            labels[i].toObject().value(QStringLiteral("name")).toString());
      }
      prHtml += QStringLiteral("<tr><th>") + i18n("Labels:") +
                QStringLiteral("</th><td>") +
                labelNames.join(QStringLiteral(", ")).toHtmlEscaped() +
                QStringLiteral("</td></tr>");
    }

    if (prInfo.contains(QStringLiteral("user"))) {
      prHtml += QStringLiteral("<tr><th>") + i18n("Author:") +
                QStringLiteral("</th><td>") +
                prInfo.value(QStringLiteral("user"))
                    .toObject()
                    .value(QStringLiteral("login"))
                    .toString()
                    .toHtmlEscaped() +
                QStringLiteral("</td></tr>");
    }

    if (prInfo.contains(QStringLiteral("head"))) {
      QString branchName = prInfo.value(QStringLiteral("head"))
                               .toObject()
                               .value(QStringLiteral("ref"))
                               .toString();
      prHtml += QStringLiteral("<tr><th>") + i18n("Branch:") +
                QStringLiteral("</th><td>") + branchName.toHtmlEscaped() +
                QStringLiteral("</td></tr>");
    }

    prHtml += QStringLiteral("</table><hr/><h3>") + i18n("Body") +
              QStringLiteral("</h3>");

    QString body = prInfo.value(QStringLiteral("body")).toString();
    if (body.isEmpty()) {
      prHtml += QStringLiteral("<p><i>") + i18n("No body provided.") +
                QStringLiteral("</i></p>");
    } else {
      // Very basic formatting for body
      prHtml += QStringLiteral("<pre style=\"white-space: pre-wrap; "
                               "font-family: sans-serif;\">") +
                body.toHtmlEscaped() + QStringLiteral("</pre>");
    }

    prHtml += QStringLiteral("</body></html>");
    if (m_prBrowser) {
      m_prBrowser->setHtml(prHtml);
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

  m_prBrowser = new QTextBrowser(this);
  m_prBrowser->setOpenExternalLinks(true);

  m_diffBrowser = new QTextBrowser(this);
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
  m_tabWidget->addTab(m_prBrowser, i18n("PR Details"));
  m_tabWidget->addTab(m_promptBrowser, i18n("Prompt"));
  m_tabWidget->addTab(m_diffBrowser, i18n("Diff"));
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
