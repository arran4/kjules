#include "sessionwindow.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include "activitybrowser.h"
#include "activitylogwindow.h"
#include "apimanager.h"
#include "clickablelabel.h"
#include "errorsmodel.h"
#include "jobpolicy.h"
#include "jobstore.h"
#include "sourcestatuswidget.h"
#include "utils.h"
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
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

SessionWindow::SessionWindow(const QString &jobId, JobStore *jobStore, APIManager *apiManager, ErrorsModel *errorsModel,
                             bool isManaged, QWidget *parent)
    : KXmlGuiWindow(parent), m_jobId(jobId), m_jobStore(jobStore), m_apiManager(apiManager), m_isManaged(isManaged),
      m_tabWidget(nullptr), m_errorsModel(errorsModel), m_statusLabel(nullptr), m_unseenErrorLabel(nullptr),
      m_autoRefreshTimer(new QTimer(this)), m_autoRefreshCombo(nullptr) {

  setObjectName(QStringLiteral("SessionWindow_Job_%1").arg(jobId));
  setAttribute(Qt::WA_DeleteOnClose);

  if (m_jobStore) {
    JobData *jobOpt = m_jobStore->getJobById(m_jobId);
    if (jobOpt && !jobOpt->attempts.isEmpty()) {
      m_currentAttemptId =
          jobOpt->acceptedAttemptId.isEmpty() ? jobOpt->attempts.first().id : jobOpt->acceptedAttemptId;
    }
  }

  setupUi(QJsonObject());
  setupActions();
  setupGUI(Default, QStringLiteral(KJULES_KXMLGUI_RESOURCE_PREFIX "sessionwindowui.rc"));

  m_autoRefreshCombo = new QComboBox(this);
  m_autoRefreshCombo->addItem(i18n("Disabled"));
  m_autoRefreshCombo->addItem(i18n("30 Seconds"));
  m_autoRefreshCombo->addItem(i18n("1 Minute"));
  m_autoRefreshCombo->addItem(i18n("5 Minutes"));
  m_autoRefreshCombo->addItem(i18n("10 Minutes"));
  m_autoRefreshCombo->addItem(i18n("30 Minutes"));
  connect(m_autoRefreshCombo, &QComboBox::currentIndexChanged, this, &SessionWindow::updateAutoRefresh);

  if (auto *tb = toolBar(QStringLiteral("mainToolBar"))) {
    QAction *closeAct = actionCollection()->action(QStringLiteral("close_window"));
    tb->insertWidget(closeAct, new QLabel(i18n(" Auto Refresh: "), this));
    tb->insertWidget(closeAct, m_autoRefreshCombo);
    tb->insertSeparator(closeAct);
    tb->show();
  }

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionWindow"));
  int autoRefreshIndex = config.readEntry("AutoRefreshIndex", 0);
  m_autoRefreshCombo->setCurrentIndex(autoRefreshIndex);
  updateAutoRefresh();

  if (m_isManaged) {
    if (m_apiManager) {
      connect(m_apiManager, &APIManager::sessionReloaded, this, &SessionWindow::onSessionReloaded);
      connect(m_apiManager, &APIManager::activitiesReceived, this, &SessionWindow::onActivitiesReceived);
      connect(m_apiManager, &APIManager::messageSent, this, &SessionWindow::onMessageSent);
      connect(m_apiManager, &APIManager::messageSendFailed, this, &SessionWindow::onMessageSendFailed);
      connect(m_apiManager, &APIManager::errorOccurred, this,
              [this](const QString &error, bool) { m_statusErrorDetails = error; });
      connect(m_apiManager, &APIManager::errorOccurredWithResponse, this,
              [this](const QString &error, const QString &, bool) { m_statusErrorDetails = error; });
    }
  }

  connect(m_autoRefreshTimer, &QTimer::timeout, this, [this]() { refreshSession(true); });
}

SessionWindow::SessionWindow(const QJsonObject &sessionData, APIManager *apiManager, ErrorsModel *errorsModel,
                             bool isManaged, QWidget *parent)
    : KXmlGuiWindow(parent), m_sessionData(sessionData), m_apiManager(apiManager), m_isManaged(isManaged),
      m_tabWidget(nullptr), m_errorsModel(errorsModel), m_statusLabel(nullptr), m_autoRefreshTimer(nullptr),
      m_autoRefreshCombo(nullptr), m_detailsBrowser(nullptr), m_promptBrowser(nullptr), m_diffBrowser(nullptr),
      m_activityBrowser(nullptr), m_rawActivitiesBrowser(nullptr) {
  setObjectName(QStringLiteral("SessionWindow_%1").arg(sessionData.value(QStringLiteral("id")).toString()));
  setAttribute(Qt::WA_DeleteOnClose);

  m_autoRefreshTimer = new QTimer(this);
  connect(m_autoRefreshTimer, &QTimer::timeout, this, [this]() { refreshSession(true); });

  if (m_apiManager) {
    connect(m_apiManager, &APIManager::sessionReloaded, this,
            [this](const QJsonObject &session, bool isBackground) { onSessionReloaded(session, isBackground); });
    connect(m_apiManager, &APIManager::activitiesReceived, this, &SessionWindow::onActivitiesReceived);
    connect(m_apiManager, &APIManager::messageSent, this, &SessionWindow::onMessageSent);
    connect(m_apiManager, &APIManager::messageSendFailed, this, &SessionWindow::onMessageSendFailed);
  }

  setupUi(m_sessionData);
  setupActions();
  setupGUI(Default, QStringLiteral(KJULES_KXMLGUI_RESOURCE_PREFIX "sessionwindowui.rc"));

  m_autoRefreshCombo = new QComboBox(this);
  m_autoRefreshCombo->addItem(i18n("Disabled"));
  m_autoRefreshCombo->addItem(i18n("30 Seconds"));
  m_autoRefreshCombo->addItem(i18n("1 Minute"));
  m_autoRefreshCombo->addItem(i18n("5 Minutes"));
  m_autoRefreshCombo->addItem(i18n("10 Minutes"));
  m_autoRefreshCombo->addItem(i18n("30 Minutes"));
  connect(m_autoRefreshCombo, &QComboBox::currentIndexChanged, this, &SessionWindow::updateAutoRefresh);

  if (auto *tb = toolBar(QStringLiteral("mainToolBar"))) {
    QAction *closeAct = actionCollection()->action(QStringLiteral("close_window"));
    tb->insertWidget(closeAct, new QLabel(i18n(" Auto Refresh: "), this));
    tb->insertWidget(closeAct, m_autoRefreshCombo);
    tb->insertSeparator(closeAct);
    tb->show();
  }

  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionWindow"));
  int autoRefreshIndex = config.readEntry("AutoRefreshIndex", 0);
  m_autoRefreshCombo->setCurrentIndex(autoRefreshIndex);
  updateAutoRefresh();
}

SessionWindow::~SessionWindow() {
  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SessionWindow"));
  config.writeEntry("AutoRefreshIndex", m_autoRefreshCombo->currentIndex());
  config.sync();
}

void SessionWindow::setupActions() {
  // Add core actions to actionCollection

  QAction *launchNewAttemptAction =
      new QAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Launch New Attempt"), this);
  connect(launchNewAttemptAction, &QAction::triggered, this, [this]() {
    // Create new attempt from job canonical
    Q_EMIT newAttemptRequested(m_jobId,
                               m_jobStore ? m_jobStore->getJobById(m_jobId)->canonicalRequest : currentSessionData());
  });
  actionCollection()->addAction(QStringLiteral("launch_new_attempt"), launchNewAttemptAction);

  QAction *launchVariantAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Launch Variant..."), this);
  connect(launchVariantAction, &QAction::triggered, this,
          [this]() { Q_EMIT variantRequested(m_jobId, currentVariantRequest()); });
  actionCollection()->addAction(QStringLiteral("launch_variant"), launchVariantAction);

  QAction *retryAttemptAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Retry Failed Attempt"), this);
  connect(retryAttemptAction, &QAction::triggered, this,
          [this]() { Q_EMIT retryAttemptRequested(m_jobId, m_currentAttemptId); });
  actionCollection()->addAction(QStringLiteral("retry_attempt"), retryAttemptAction);

  QAction *newJobFromAction =
      new QAction(QIcon::fromTheme(QStringLiteral("window-new")), i18n("New Job From This..."), this);
  connect(newJobFromAction, &QAction::triggered, this, [this]() { Q_EMIT newJobFromRequested(currentSessionData()); });
  actionCollection()->addAction(QStringLiteral("new_job_from"), newJobFromAction);

  QAction *chooseWinnerAction =
      new QAction(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), i18n("Choose as Winner"), this);
  connect(chooseWinnerAction, &QAction::triggered, this, [this]() {
    if (m_jobStore && !m_jobId.isEmpty() && !m_currentAttemptId.isEmpty()) {
      JobData *job = m_jobStore->getJobById(m_jobId);
      if (job) {
        JobData updatedJob = *job;
        updatedJob.acceptedAttemptId = m_currentAttemptId;
        if (m_jobStore->updateJobTransactional(updatedJob)) {
          updateAttemptList();
          Q_EMIT jobMutated(m_jobId);
        } else {
          QMessageBox::warning(this, i18n("Error"), i18n("Failed to save winner selection to disk."));
        }
      }
    }
  });
  actionCollection()->addAction(QStringLiteral("choose_winner"), chooseWinnerAction);

  QAction *archiveJobAction = new QAction(QIcon::fromTheme(QStringLiteral("archive")), i18n("Archive Job"), this);
  connect(archiveJobAction, &QAction::triggered, this, [this]() {
    if (m_jobStore) {
      Q_EMIT archiveRequested(m_jobId); // Overloaded to take job ID if it exists?
                                        // MainWindow uses Session ID right now.
    } else {
      Q_EMIT archiveRequested(currentSessionData().value(QStringLiteral("id")).toString());
    }
  });
  actionCollection()->addAction(QStringLiteral("archive_job"), archiveJobAction);

  QAction *deleteJobAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete Job"), this);
  connect(deleteJobAction, &QAction::triggered, this, [this]() {
    if (QMessageBox::question(
            this, i18n("Confirm Delete"),
            i18n("Are you sure you want to delete this Job and all its attempts? This cannot be undone.")) ==
        QMessageBox::Yes) {
      if (m_jobStore) {
        if (m_jobStore->removeJobTransactional(m_jobId)) {
          Q_EMIT jobMutated(m_jobId);
          close();
        } else {
          QMessageBox::warning(this, i18n("Error"), i18n("Failed to delete Job from disk."));
        }
      }
    }
  });
  actionCollection()->addAction(QStringLiteral("delete_job"), deleteJobAction);

  // Re-map the existing Zero Attempt layout buttons
  QAction *refreshAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
  actionCollection()->addAction(QStringLiteral("refresh_session"), refreshAction);
  actionCollection()->setDefaultShortcut(refreshAction, QKeySequence(Qt::Key_F5));
  connect(refreshAction, &QAction::triggered, this, &SessionWindow::refreshSession);

  QAction *duplicateAction =
      new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Duplicate Session"), this);
  actionCollection()->addAction(QStringLiteral("duplicate_session"), duplicateAction);
  connect(duplicateAction, &QAction::triggered, this, &SessionWindow::duplicateSession);

  QAction *closeAction = new QAction(QIcon::fromTheme(QStringLiteral("window-close")), i18n("Close"), this);
  actionCollection()->addAction(QStringLiteral("close_window"), closeAction);
  actionCollection()->setDefaultShortcut(closeAction, QKeySequence(Qt::CTRL | Qt::Key_W));
  connect(closeAction, &QAction::triggered, this, &SessionWindow::close);

  // m_autoRefreshCombo is created in the constructor so we only connect it here

  QAction *saveTemplateAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Save prompt as template"), this);
  connect(saveTemplateAction, &QAction::triggered, this, [this]() {
    QJsonObject templateData;
    templateData[QStringLiteral("prompt")] = currentSessionData().value(QStringLiteral("prompt")).toString();
    templateData[QStringLiteral("automationMode")] =
        currentSessionData().value(QStringLiteral("automationMode")).toString();
    Q_EMIT templateRequested(templateData);
  });
  actionCollection()->addAction(QStringLiteral("save_template"), saveTemplateAction);

  QAction *watchAction = new QAction(QIcon::fromTheme(QStringLiteral("visibility")), i18n("Follow Session"), this);
  connect(watchAction, &QAction::triggered, this, [this, watchAction]() {
    Q_EMIT watchRequested(currentSessionData());
    m_isManaged = true;
    watchAction->setEnabled(false);
  });
  actionCollection()->addAction(QStringLiteral("watch_session"), watchAction);
  if (m_isManaged) {
    watchAction->setEnabled(false);
  }

  QAction *archiveAction = new QAction(QIcon::fromTheme(QStringLiteral("archive")), i18n("Archive Session"), this);
  connect(archiveAction, &QAction::triggered, this,
          [this]() { Q_EMIT archiveRequested(currentSessionData().value(QStringLiteral("id")).toString()); });
  actionCollection()->addAction(QStringLiteral("archive_session"), archiveAction);
  if (!m_isManaged) {
    archiveAction->setEnabled(false);
  }

  QAction *deleteAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Unmanage Session"), this);
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this,
          [this]() { Q_EMIT deleteRequested(currentSessionData().value(QStringLiteral("id")).toString()); });
  actionCollection()->addAction(QStringLiteral("delete_session"), deleteAction);
  if (!m_isManaged) {
    deleteAction->setEnabled(false);
  }

  QAction *openJulesAction = new QAction(i18n("Open Jules URL"), this);
  connect(openJulesAction, &QAction::triggered, this, [this]() {
    QString id = currentSessionData().value(QStringLiteral("id")).toString();
    Utils::openUrl(QUrl(QStringLiteral("https://jules.google.com/session/") + id));
  });
  actionCollection()->addAction(QStringLiteral("open_jules"), openJulesAction);

  QAction *copyJulesAction = new QAction(i18n("Copy Jules URL"), this);
  connect(copyJulesAction, &QAction::triggered, this, [this]() {
    QString id = currentSessionData().value(QStringLiteral("id")).toString();
    QGuiApplication::clipboard()->setText(QStringLiteral("https://jules.google.com/session/") + id);
  });
  actionCollection()->addAction(QStringLiteral("copy_jules"), copyJulesAction);

  QString prUrlStr;
  QJsonArray outputs = currentSessionData().value(QStringLiteral("outputs")).toArray();
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outObj = outputs[i].toObject();
    if (outObj.contains(QStringLiteral("pullRequest"))) {
      prUrlStr = outObj.value(QStringLiteral("pullRequest")).toObject().value(QStringLiteral("url")).toString();
      if (prUrlStr == QLatin1StringView("undefined")) {
        prUrlStr.clear();
      }
    }
  }

  if (!prUrlStr.isEmpty()) {
    QAction *openPrAction = new QAction(i18n("Open Pull Request URL"), this);
    connect(openPrAction, &QAction::triggered, this, [prUrlStr]() { Utils::openUrl(QUrl(prUrlStr)); });
    actionCollection()->addAction(QStringLiteral("open_pr"), openPrAction);

    QAction *copyPrAction = new QAction(i18n("Copy Pull Request URL"), this);
    connect(copyPrAction, &QAction::triggered, this, [prUrlStr]() { QGuiApplication::clipboard()->setText(prUrlStr); });
    actionCollection()->addAction(QStringLiteral("copy_pr"), copyPrAction);

    if (currentSessionData().contains(QStringLiteral("githubPrInfo"))) {
      QJsonObject prInfo = currentSessionData().value(QStringLiteral("githubPrInfo")).toObject();
      if (prInfo.contains(QStringLiteral("head"))) {
        QString branchName = prInfo.value(QStringLiteral("head")).toObject().value(QStringLiteral("ref")).toString();
        QString branchUrl = prInfo.value(QStringLiteral("head"))
                                .toObject()
                                .value(QStringLiteral("repo"))
                                .toObject()
                                .value(QStringLiteral("html_url"))
                                .toString() +
                            QStringLiteral("/tree/") + branchName;

        QAction *openBranchAction = new QAction(i18n("Open Branch URL"), this);
        connect(openBranchAction, &QAction::triggered, this, [branchUrl]() { Utils::openUrl(QUrl(branchUrl)); });
        actionCollection()->addAction(QStringLiteral("open_branch"), openBranchAction);

        QAction *copyBranchAction = new QAction(i18n("Copy Branch URL"), this);
        connect(copyBranchAction, &QAction::triggered, this,
                [branchUrl]() { QGuiApplication::clipboard()->setText(branchUrl); });
        actionCollection()->addAction(QStringLiteral("copy_branch"), copyBranchAction);
      }
    }
  }

  m_statusLabel = new ClickableLabel(i18n("Ready"), this);
  m_statusLabel->setObjectName(QStringLiteral("sessionStatusLabel"));
  connect(m_statusLabel, &ClickableLabel::clicked, this, []() {
    ActivityLogWindow::instance()->show();
    ActivityLogWindow::instance()->raise();
    ActivityLogWindow::instance()->activateWindow();
  });

  connect(m_statusLabel, &QLabel::linkActivated, this, [this](const QString &link) {
    if (link == QStringLiteral("#error-details")) {
      if (m_textBrowser) {
        m_textBrowser->setPlainText(m_statusErrorDetails);
        m_tabWidget->setCurrentWidget(m_textBrowser);
      }
    }
  });

  statusBar()->addWidget(m_statusLabel);

  if (m_apiManager) {
    m_statusLabel->setText(i18n("Loading activities..."));
  }

  updateActionStates();
}

void SessionWindow::updateActionStates() {
  bool isArchived = false;
  if (m_jobStore && !m_jobId.isEmpty()) {
    JobData *job = m_jobStore->getJobById(m_jobId);
    if (job) {
      isArchived = (JobPolicy::aggregateState(*job) == JobPolicy::JobAggregateState::Archived);
    }
  }

  if (auto *act = actionCollection()->action(QStringLiteral("launch_new_attempt"))) {
    act->setEnabled(!isArchived);
  }
  if (auto *act = actionCollection()->action(QStringLiteral("launch_variant"))) {
    act->setEnabled(!isArchived);
  }
  if (auto *act = actionCollection()->action(QStringLiteral("retry_attempt"))) {
    act->setEnabled(!isArchived);
  }
  if (auto *act = actionCollection()->action(QStringLiteral("choose_winner"))) {
    act->setEnabled(!isArchived);
  }
  if (auto *act = actionCollection()->action(QStringLiteral("archive_job"))) {
    act->setEnabled(!isArchived);
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

void SessionWindow::refreshSession(bool isBackground) {
  if (m_apiManager) {
    QString id = currentSessionData().value(QStringLiteral("id")).toString();
    m_apiManager->reloadSession(id, isBackground);
    m_statusLabel->setText(i18n("Refreshing..."));
  }
}

void SessionWindow::onSessionReloaded(const QJsonObject &session, bool isBackground) {
  QString currentId = currentSessionData().value(QStringLiteral("id")).toString();
  if (session.value(QStringLiteral("id")).toString() == currentId) {
    if (m_jobStore) {
      JobData *job = m_jobStore->getJobById(m_jobId);
      if (job) {
        for (auto &attempt : job->attempts) {
          if (attempt.id == m_currentAttemptId) {
            attempt.rawResponse = session;
            attempt.julesState = session.value(QStringLiteral("state")).toString();
            m_jobStore->save();
            break;
          }
        }
      }
    } else {
      m_sessionData = session;
      m_sessionData[QStringLiteral("lastRefreshed")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }

    if (m_statusLabel)
      m_statusLabel->setText(i18n("Refreshed at %1", QDateTime::currentDateTime().toString(
                                                         QLocale::system().dateFormat(QLocale::ShortFormat))));

    renderDetailsAndDiff();
  }
}

void SessionWindow::onMessageSent(const QString &sessionId) {
  QString currentId = currentSessionData().value(QStringLiteral("id")).toString();
  if (currentId != sessionId)
    return;

  m_pendingMessage.clear();
  m_statusErrorDetails.clear();
  if (m_chatInput) {
    m_chatInput->setEnabled(true);
    m_chatInput->setFocus();
  }
  if (m_sendButton) {
    m_sendButton->setEnabled(true);
  }

  if (m_statusLabel) {
    m_statusLabel->setText(i18n("Message sent. Refreshing..."));
  }

  if (m_apiManager) {
    m_apiManager->listActivities(currentId);
  }
}

void SessionWindow::onMessageSendFailed(const QString &sessionId, const QString &message, const QString &httpDetails) {
  QString currentId = currentSessionData().value(QStringLiteral("id")).toString();
  if (currentId != sessionId)
    return;

  if (m_chatInput) {
    m_chatInput->setEnabled(true);
    if (m_chatInput->text().isEmpty() && !m_pendingMessage.isEmpty()) {
      m_chatInput->setText(m_pendingMessage);
    }
  }
  if (m_sendButton) {
    m_sendButton->setEnabled(true);
  }

  if (m_statusLabel) {
    QString errorText = message;
    if (!httpDetails.isEmpty()) {
      m_statusErrorDetails = httpDetails;
      m_statusLabel->setText(i18n("Failed to send message: %1 <a href=\"#error-details\">[Details]</a>", errorText));
      m_statusLabel->setTextFormat(Qt::RichText);
      m_statusLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    } else {
      m_statusErrorDetails.clear();
      m_statusLabel->setText(i18n("Failed to send message: %1", errorText));
      m_statusLabel->setTextFormat(Qt::PlainText);
    }
  }
}

void SessionWindow::onActivitiesReceived(const QString &sessionId, const QJsonArray &activities) {
  QString currentId = currentSessionData().value(QStringLiteral("id")).toString();
  if (currentId != sessionId)
    return;

  QJsonArray turns = activities;
  if (turns.isEmpty()) {
    if (currentSessionData().contains(QStringLiteral("turns"))) {
      turns = currentSessionData().value(QStringLiteral("turns")).toArray();
    } else if (currentSessionData().contains(QStringLiteral("history"))) {
      turns = currentSessionData().value(QStringLiteral("history")).toArray();
    } else if (currentSessionData().contains(QStringLiteral("messages"))) {
      turns = currentSessionData().value(QStringLiteral("messages")).toArray();
    } else if (currentSessionData().contains(QStringLiteral("actions"))) {
      turns = currentSessionData().value(QStringLiteral("actions")).toArray();
    }
  }

  QString prompt = currentSessionData().value(QStringLiteral("prompt")).toString();
  m_activityBrowser->setPrompt(prompt);
  m_activityBrowser->setActivities(turns);

  QJsonDocument activitiesDoc(turns);
  m_rawActivitiesBrowser->setPlainText(QString::fromUtf8(activitiesDoc.toJson(QJsonDocument::Indented)));

  if (m_statusLabel) {
    m_statusLabel->setText(i18n(
        "Refreshed at %1", QDateTime::currentDateTime().toString(QLocale::system().dateFormat(QLocale::ShortFormat))));
  }
}

QJsonObject SessionWindow::currentSessionData() const {
  if (m_jobStore) {
    JobData *job = m_jobStore->getJobById(m_jobId);
    if (job) {
      for (const auto &attempt : job->attempts) {
        if (attempt.id == m_currentAttemptId) {
          QJsonObject data = attempt.requestSnapshot;
          QJsonObject outputs;
          if (attempt.rawResponse.contains(QStringLiteral("outputs"))) {
            outputs = attempt.rawResponse;
          }

          // Keep ID to allow refresh etc
          data[QStringLiteral("id")] = attempt.julesSessionId;
          data[QStringLiteral("state")] = attempt.julesState;

          if (!outputs.isEmpty()) {
            data[QStringLiteral("outputs")] = outputs.value(QStringLiteral("outputs"));
          }
          if (attempt.rawResponse.contains(QStringLiteral("turns"))) {
            data[QStringLiteral("turns")] = attempt.rawResponse.value(QStringLiteral("turns"));
          }
          if (attempt.createdAt.isValid()) {
            data[QStringLiteral("createTime")] = attempt.createdAt.toUTC().toString(Qt::ISODate);
            data[QStringLiteral("createdAt")] = attempt.createdAt.toUTC().toString(Qt::ISODate);
          }
          if (attempt.updatedAt.isValid()) {
            data[QStringLiteral("updateTime")] = attempt.updatedAt.toUTC().toString(Qt::ISODate);
            data[QStringLiteral("updatedAt")] = attempt.updatedAt.toUTC().toString(Qt::ISODate);
          }

          if (attempt.rawResponse.contains(QStringLiteral("githubPrInfo"))) {
            data[QStringLiteral("githubPrInfo")] = attempt.rawResponse.value(QStringLiteral("githubPrInfo"));
          }

          // Use canonical request for display title etc if missing in snapshot
          if (!data.contains(QStringLiteral("title"))) {
            data[QStringLiteral("title")] = job->canonicalRequest.value(QStringLiteral("title"));
          }

          return data;
        }
      }
    }
  }
  return m_sessionData;
}

QJsonObject SessionWindow::currentVariantRequest() const {
  if (m_jobStore && !m_jobId.isEmpty()) {
    JobData *job = m_jobStore->getJobById(m_jobId);
    if (job) {
      if (!m_currentAttemptId.isEmpty()) {
        for (const auto &attempt : job->attempts) {
          if (attempt.id == m_currentAttemptId && !attempt.requestSnapshot.isEmpty()) {
            return attempt.requestSnapshot;
          }
        }
      }
      return job->canonicalRequest;
    }
  }
  return currentSessionData();
}

void SessionWindow::renderDetailsAndDiff() {
  QJsonObject data = currentSessionData();
  QJsonDocument doc(data);
  QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
  m_textBrowser->setPlainText(jsonString);

  QString title = currentSessionData().value(QStringLiteral("title")).toString();
  QString sessionId = currentSessionData().value(QStringLiteral("id")).toString();
  QString lastRefreshed = currentSessionData().value(QStringLiteral("lastRefreshed")).toString();
  QString state = currentSessionData().value(QStringLiteral("state")).toString();
  QJsonObject sourceContext = currentSessionData().value(QStringLiteral("sourceContext")).toObject();
  QString source = sourceContext.value(QStringLiteral("source")).toString();
  bool environmentVariablesEnabled = sourceContext.value(QStringLiteral("environmentVariablesEnabled")).toBool();
  QString startingBranch = sourceContext.value(QStringLiteral("githubRepoContext"))
                               .toObject()
                               .value(QStringLiteral("startingBranch"))
                               .toString();
  QString createTime = currentSessionData().value(QStringLiteral("createTime")).toString();
  QString updateTime = currentSessionData().value(QStringLiteral("updateTime")).toString();
  QString promptText = currentSessionData().value(QStringLiteral("prompt")).toString();

  if (!createTime.isEmpty()) {
    QDateTime dt = QDateTime::fromString(createTime, Qt::ISODate);
    if (dt.isValid()) {
      createTime = dt.toLocalTime().toString(QLocale::system().dateFormat(QLocale::ShortFormat));
    }
  }
  if (!updateTime.isEmpty()) {
    QDateTime dt = QDateTime::fromString(updateTime, Qt::ISODate);
    if (dt.isValid()) {
      updateTime = dt.toLocalTime().toString(QLocale::system().dateFormat(QLocale::ShortFormat));
    }
  }

  QString detailsHtml = QStringLiteral("<html><head><style>") +
                        QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                                       "line-height: 1.6; }") +
                        QStringLiteral("th { text-align: left; padding-right: 15px; color: #555; }") +
                        QStringLiteral("a { color: #3498db; text-decoration: none; }") +
                        QStringLiteral("a:hover { text-decoration: underline; }") +
                        QStringLiteral("</style></head><body><h2>") + i18n("Session Details") +
                        QStringLiteral("</h2><table>");

  QString julesUrl = QStringLiteral("https://jules.google.com/session/") + sessionId;
  detailsHtml += QStringLiteral("<tr><th>") + i18n("ID:") + QStringLiteral("</th><td>") + sessionId.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Jules URL:") + QStringLiteral("</th><td><a href=\"") +
                 julesUrl.toHtmlEscaped() + QStringLiteral("\">") + julesUrl.toHtmlEscaped() +
                 QStringLiteral("</a></td></tr>");
  detailsHtml += QStringLiteral("<tr><th>") + i18n("State:") + QStringLiteral("</th><td>") + state.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");

  QString previousAttemptId;
  QJsonObject req = currentSessionData().value(QStringLiteral("request")).toObject();
  if (currentSessionData().contains(QStringLiteral("previousAttemptId"))) {
    previousAttemptId = currentSessionData().value(QStringLiteral("previousAttemptId")).toString();
  } else if (req.contains(QStringLiteral("previousAttemptId"))) {
    previousAttemptId = req.value(QStringLiteral("previousAttemptId")).toString();
  }

  if (!previousAttemptId.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Previous Attempt:") +
                   QStringLiteral("</th><td><a href=\"previous://") + previousAttemptId.toHtmlEscaped() +
                   QStringLiteral("\">") + previousAttemptId.toHtmlEscaped() + QStringLiteral("</a></td></tr>");
  }

  detailsHtml += QStringLiteral("<tr><th>") + i18n("Source:") + QStringLiteral("</th><td>") + source.toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  if (!startingBranch.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Starting Branch:") + QStringLiteral("</th><td>") +
                   startingBranch.toHtmlEscaped() + QStringLiteral("</td></tr>");
  }
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Env Vars Enabled:") + QStringLiteral("</th><td>") +
                 (environmentVariablesEnabled ? i18n("Yes") : i18n("No")) + QStringLiteral("</td></tr>");
  if (!createTime.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Create Time:") + QStringLiteral("</th><td>") +
                   createTime.toHtmlEscaped() + QStringLiteral("</td></tr>");
  }
  if (!updateTime.isEmpty()) {
    detailsHtml += QStringLiteral("<tr><th>") + i18n("Update Time:") + QStringLiteral("</th><td>") +
                   updateTime.toHtmlEscaped() + QStringLiteral("</td></tr>");
  }
  detailsHtml += QStringLiteral("<tr><th>") + i18n("Last Refreshed:") + QStringLiteral("</th><td>") +
                 (lastRefreshed.isEmpty() ? i18n("Never") : lastRefreshed).toHtmlEscaped() +
                 QStringLiteral("</td></tr>");
  detailsHtml += QStringLiteral("</table>");

  QJsonArray outputs = currentSessionData().value(QStringLiteral("outputs")).toArray();
  QString diffText;
  for (int i = 0; i < outputs.size(); ++i) {
    QJsonObject outObj = outputs[i].toObject();
    if (outObj.contains(QStringLiteral("pullRequest"))) {
      QJsonObject prObj = outObj.value(QStringLiteral("pullRequest")).toObject();
      QString prUrl = prObj.value(QStringLiteral("url")).toString();
      if (prUrl == QLatin1StringView("undefined")) {
        prUrl.clear();
      }
      if (!prUrl.isEmpty()) {
        QString prTitle = prObj.value(QStringLiteral("title")).toString();
        detailsHtml += QStringLiteral("<hr/><h3>") + i18n("Pull Request") + QStringLiteral("</h3><table>");
        detailsHtml += QStringLiteral("<tr><th>") + i18n("Title:") + QStringLiteral("</th><td>") +
                       prTitle.toHtmlEscaped() + QStringLiteral("</td></tr>");
        detailsHtml += QStringLiteral("<tr><th>") + i18n("URL:") + QStringLiteral("</th><td><a href=\"") +
                       prUrl.toHtmlEscaped() + QStringLiteral("\">") + prUrl.toHtmlEscaped() +
                       QStringLiteral("</a></td></tr>");
        detailsHtml += QStringLiteral("</table>");
      }
    }
    if (outObj.contains(QStringLiteral("changeSet"))) {
      QJsonObject changeSet = outObj.value(QStringLiteral("changeSet")).toObject();
      if (changeSet.contains(QStringLiteral("gitPatch"))) {
        QJsonObject gitPatch = changeSet.value(QStringLiteral("gitPatch")).toObject();
        diffText = gitPatch.value(QStringLiteral("unidiffPatch")).toString();
      }
    }
  }

  detailsHtml += QStringLiteral("</body></html>");

  m_detailsBrowser->setHtml(detailsHtml);

  if (currentSessionData().contains(QStringLiteral("githubPrInfo"))) {
    QJsonObject prInfo = currentSessionData().value(QStringLiteral("githubPrInfo")).toObject();
    QString prHtml = QStringLiteral("<html><head><style>") +
                     QStringLiteral("body { font-family: sans-serif; font-size: 1.1em; "
                                    "line-height: 1.6; }") +
                     QStringLiteral("th { text-align: left; padding-right: 15px; color: #555; }") +
                     QStringLiteral("a { color: #3498db; text-decoration: none; }") +
                     QStringLiteral("a:hover { text-decoration: underline; }") +
                     QStringLiteral("</style></head><body><h2>") + i18n("Pull Request Summary") +
                     QStringLiteral("</h2><table>");

    prHtml += QStringLiteral("<tr><th>") + i18n("Title:") + QStringLiteral("</th><td>") +
              prInfo.value(QStringLiteral("title")).toString().toHtmlEscaped() + QStringLiteral("</td></tr>");
    QString state = prInfo.value(QStringLiteral("state")).toString();
    if (prInfo.value(QStringLiteral("merged_at")).isString()) {
      state = QStringLiteral("merged");
    }
    prHtml += QStringLiteral("<tr><th>") + i18n("State:") + QStringLiteral("</th><td>") + state.toHtmlEscaped() +
              QStringLiteral("</td></tr>");

    QJsonArray labels = prInfo.value(QStringLiteral("labels")).toArray();
    if (!labels.isEmpty()) {
      QStringList labelNames;
      for (int i = 0; i < labels.size(); ++i) {
        labelNames.append(labels[i].toObject().value(QStringLiteral("name")).toString());
      }
      prHtml += QStringLiteral("<tr><th>") + i18n("Labels:") + QStringLiteral("</th><td>") +
                labelNames.join(QStringLiteral(", ")).toHtmlEscaped() + QStringLiteral("</td></tr>");
    }

    if (prInfo.contains(QStringLiteral("user"))) {
      prHtml +=
          QStringLiteral("<tr><th>") + i18n("Author:") + QStringLiteral("</th><td>") +
          prInfo.value(QStringLiteral("user")).toObject().value(QStringLiteral("login")).toString().toHtmlEscaped() +
          QStringLiteral("</td></tr>");
    }

    if (prInfo.contains(QStringLiteral("head"))) {
      QString branchName = prInfo.value(QStringLiteral("head")).toObject().value(QStringLiteral("ref")).toString();
      prHtml += QStringLiteral("<tr><th>") + i18n("Branch:") + QStringLiteral("</th><td>") +
                branchName.toHtmlEscaped() + QStringLiteral("</td></tr>");
    }

    prHtml += QStringLiteral("</table><hr/><h3>") + i18n("Body") + QStringLiteral("</h3>");

    QString body = prInfo.value(QStringLiteral("body")).toString();
    if (body.isEmpty()) {
      prHtml += QStringLiteral("<p><i>") + i18n("No body provided.") + QStringLiteral("</i></p>");
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

void SessionWindow::duplicateSession() { Q_EMIT duplicateRequested(currentSessionData()); }

void SessionWindow::setupUi(const QJsonObject &sessionData) {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  mainLayout->addWidget(m_splitter);

  m_attemptList = new QListWidget(this);
  m_splitter->addWidget(m_attemptList);

  connect(m_attemptList, &QListWidget::itemClicked, this, &SessionWindow::onAttemptSelected);
  connect(m_attemptList, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *) { onAttemptSelected(current); });

  m_contentStack = new QStackedWidget(this);
  m_splitter->addWidget(m_contentStack);

  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 4);

  m_zeroAttemptWidget = new QWidget(this);
  new QVBoxLayout(m_zeroAttemptWidget);
  // It is populated by renderZeroAttempts()
  m_contentStack->addWidget(m_zeroAttemptWidget);

  m_detailsWidget = new QWidget(this);
  QVBoxLayout *detailsLayout = new QVBoxLayout(m_detailsWidget);

  m_tabWidget = new QTabWidget(this);
  detailsLayout->addWidget(m_tabWidget);
  m_contentStack->addWidget(m_detailsWidget);

  m_detailsBrowser = new QTextBrowser(this);
  m_detailsBrowser->setOpenExternalLinks(false);
  connect(m_detailsBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl &link) {
    if (link.scheme() == QStringLiteral("previous")) {
      Q_EMIT openPreviousAttemptRequested(link.host());
    } else {
      QDesktopServices::openUrl(link);
    }
  });

  m_promptBrowser = new QTextBrowser(this);

  m_prBrowser = new QTextBrowser(this);
  m_prBrowser->setOpenExternalLinks(true);

  m_diffBrowser = new QTextBrowser(this);
  m_diffBrowser->setStyleSheet(QStringLiteral("font-family: monospace;"));

  m_rawActivitiesBrowser = new QTextBrowser(this);
  m_activityBrowser = new ActivityBrowser(this);
  connect(m_activityBrowser, &ActivityBrowser::duplicateRequested, this, &SessionWindow::duplicateSession);
  m_textBrowser = new QTextBrowser(this);

  m_activityTabWidget = new QWidget(this);
  QVBoxLayout *activityLayout = new QVBoxLayout(m_activityTabWidget);
  activityLayout->addWidget(m_activityBrowser);

  QHBoxLayout *chatInputLayout = new QHBoxLayout();
  m_chatInput = new QLineEdit(this);
  m_chatInput->setPlaceholderText(i18n("Type a message..."));
  m_sendButton = new QPushButton(QIcon::fromTheme(QStringLiteral("mail-send")), i18n("Send"), this);
  chatInputLayout->addWidget(m_chatInput);
  chatInputLayout->addWidget(m_sendButton);
  activityLayout->addLayout(chatInputLayout);

  connect(m_sendButton, &QPushButton::clicked, this, [this]() {
    QString text = m_chatInput->text().trimmed();
    if (text.isEmpty() || !m_apiManager)
      return;

    QString id = currentSessionData().value(QStringLiteral("id")).toString();
    m_pendingMessage = text;
    m_chatInput->clear();
    m_chatInput->setEnabled(false);
    m_sendButton->setEnabled(false);

    if (m_statusLabel) {
      m_statusLabel->setText(i18n("Sending message..."));
    }

    m_apiManager->sendMessage(id, text);
  });
  connect(m_chatInput, &QLineEdit::returnPressed, m_sendButton, &QPushButton::click);

  m_tabWidget->addTab(m_detailsBrowser, i18n("Details"));
  m_tabWidget->addTab(m_prBrowser, i18n("PR Details"));
  m_tabWidget->addTab(m_promptBrowser, i18n("Prompt"));
  m_tabWidget->addTab(m_diffBrowser, i18n("Diff"));
  m_tabWidget->addTab(m_activityTabWidget, i18n("Activity Feed"));
  m_tabWidget->addTab(m_rawActivitiesBrowser, i18n("Raw Activities"));
  m_tabWidget->addTab(m_textBrowser, i18n("Raw JSON"));

  m_errorTab = new QWidget(this);
  QVBoxLayout *errorLayout = new QVBoxLayout(m_errorTab);
  QListView *errorView = new QListView(m_errorTab);
  SessionErrorFilterProxyModel *errorProxy =
      new SessionErrorFilterProxyModel(currentSessionData().value(QStringLiteral("id")).toString(), m_errorTab);
  errorProxy->setObjectName(QStringLiteral("errorProxy")); // Important for later updates
  if (m_jobStore) {
    errorProxy->setFilterTarget(m_jobId, m_currentAttemptId,
                                currentSessionData().value(QStringLiteral("id")).toString());
  }
  errorProxy->setSourceModel(m_errorsModel);
  errorView->setModel(errorProxy);
  errorLayout->addWidget(errorView);
  m_tabWidget->addTab(m_errorTab, i18n("Errors"));

  m_unseenErrorLabel = new ClickableLabel(this);
  m_unseenErrorLabel->hide();

  auto updateUnseenErrors = [this, errorProxy]() {
    if (!m_errorsModel || !m_unseenErrorLabel)
      return;
    int unseenCount = 0;
    for (int i = 0; i < errorProxy->rowCount(); ++i) {
      if (errorProxy->data(errorProxy->index(i, 0), ErrorsModel::UnseenRole).toBool()) {
        unseenCount++;
      }
    }
    if (unseenCount > 0) {
      m_unseenErrorLabel->setText(i18np("1 Unseen Error", "%1 Unseen Errors", unseenCount));
      m_unseenErrorLabel->show();
    } else {
      m_unseenErrorLabel->hide();
    }
  };

  if (m_errorsModel) {
    connect(m_errorsModel, &ErrorsModel::dataChanged, this, updateUnseenErrors);
    connect(m_errorsModel, &ErrorsModel::rowsInserted, this, updateUnseenErrors);
    connect(m_errorsModel, &ErrorsModel::rowsRemoved, this, updateUnseenErrors);
    connect(m_errorsModel, &ErrorsModel::modelReset, this, updateUnseenErrors);
    updateUnseenErrors();
  }

  connect(m_tabWidget, &QTabWidget::currentChanged, this, [this, errorProxy](int index) {
    if (m_tabWidget->widget(index) == m_errorTab && m_errorsModel) {
      for (int i = 0; i < errorProxy->rowCount(); ++i) {
        if (errorProxy->data(errorProxy->index(i, 0), ErrorsModel::UnseenRole).toBool()) {
          QModelIndex sourceIndex = errorProxy->mapToSource(errorProxy->index(i, 0));
          m_errorsModel->markSeen(sourceIndex.row());
        }
      }
    }
  });

  connect(m_unseenErrorLabel, &ClickableLabel::clicked, this, [this]() { m_tabWidget->setCurrentWidget(m_errorTab); });

  statusBar()->addWidget(m_unseenErrorLabel);

  QString title;
  QString sessionId;

  if (m_jobStore) {
    JobData *jobOpt = m_jobStore->getJobById(m_jobId);
    if (jobOpt) {
      title = jobOpt->canonicalRequest.value(QStringLiteral("title")).toString();
    }
    sessionId = m_currentAttemptId;
  } else {
    title = sessionData.value(QStringLiteral("title")).toString();
    sessionId = sessionData.value(QStringLiteral("id")).toString();
  }

  if (title.isEmpty()) {
    title = i18n("Details");
  }
  setWindowTitle(i18n("Session %1 - %2", sessionId, title));

  if (m_jobStore) {
    updateAttemptList();
  } else {
    m_splitter->widget(0)->hide();
    m_contentStack->setCurrentWidget(m_detailsWidget);
    renderDetailsAndDiff();
  }

  if (m_apiManager && !sessionId.isEmpty()) {
    if (m_statusLabel)
      m_statusLabel->setText(i18n("Loading activities..."));
    m_apiManager->listActivities(sessionId);
  } else if (!sessionId.isEmpty()) {
    onActivitiesReceived(sessionId, QJsonArray());
  }

  resize(800, 600);
}

void SessionWindow::renderZeroAttempts() {
  if (!m_jobStore)
    return;
  JobData *jobOpt = m_jobStore->getJobById(m_jobId);
  if (!jobOpt)
    return;

  JobData job = *jobOpt;

  QLayout *l = m_zeroAttemptWidget->layout();
  if (l) {
    QLayoutItem *item;
    while ((item = l->takeAt(0)) != nullptr) {
      delete item->widget();
      delete item;
    }
    delete l;
  }

  QVBoxLayout *zeroLayout = new QVBoxLayout(m_zeroAttemptWidget);

  QLabel *titleLabel = new QLabel(
      i18n("<b>Job:</b> %1", job.canonicalRequest.value(QStringLiteral("title")).toString()), m_zeroAttemptWidget);
  titleLabel->setObjectName(QStringLiteral("zeroTitleLabel"));
  zeroLayout->addWidget(titleLabel);

  QLabel *idLabel = new QLabel(i18n("<b>ID:</b> %1", job.id), m_zeroAttemptWidget);
  idLabel->setObjectName(QStringLiteral("zeroIdLabel"));
  zeroLayout->addWidget(idLabel);

  QJsonObject sourceContext = job.canonicalRequest.value(QStringLiteral("sourceContext")).toObject();
  QString source = sourceContext.value(QStringLiteral("source")).toString();
  QLabel *sourceLabel = new QLabel(i18n("<b>Source:</b> %1", source), m_zeroAttemptWidget);
  sourceLabel->setObjectName(QStringLiteral("zeroSourceLabel"));
  zeroLayout->addWidget(sourceLabel);

  QString branch = sourceContext.value(QStringLiteral("githubRepoContext"))
                       .toObject()
                       .value(QStringLiteral("startingBranch"))
                       .toString();
  if (!branch.isEmpty()) {
    QLabel *branchLabel = new QLabel(i18n("<b>Branch:</b> %1", branch), m_zeroAttemptWidget);
    branchLabel->setObjectName(QStringLiteral("zeroBranchLabel"));
    zeroLayout->addWidget(branchLabel);
  }

  zeroLayout->addSpacing(8);

  bool isArchived = (JobPolicy::aggregateState(job) == JobPolicy::JobAggregateState::Archived);
  bool isHolding = job.legacyMetadata.value(QStringLiteral("holding")).toBool();
  bool isBlocked = job.legacyMetadata.value(QStringLiteral("blocked")).toBool();

  QString schedState;
  if (isArchived) {
    schedState = i18n("Archived");
  } else if (isHolding) {
    schedState = i18n("Holding");
  } else if (isBlocked) {
    schedState = i18n("Queued (Blocked by concurrency)");
  } else {
    schedState = i18n("Queued (Pending dispatch)");
  }

  QString reasonNoSession;
  if (job.lifecycleMetadata.contains(QStringLiteral("reason"))) {
    reasonNoSession = job.lifecycleMetadata.value(QStringLiteral("reason")).toString();
  } else if (isArchived) {
    reasonNoSession = i18n("Job is archived; no active attempts exist.");
  } else if (isHolding) {
    reasonNoSession = i18n("Job is held in holding queue; dispatch is paused.");
  } else if (isBlocked) {
    reasonNoSession = i18n("Job is waiting for concurrency limit availability.");
  } else if (job.lifecycleMetadata.value(QStringLiteral("status")).toString() == QStringLiteral("ERROR_STATE")) {
    reasonNoSession = i18n("Previous dispatch attempt failed before a remote Jules session could be established.");
  } else {
    reasonNoSession = i18n("Job is awaiting initial dispatch to Jules.");
  }

  QString lastErrorText;
  if (job.lifecycleMetadata.contains(QStringLiteral("lastError"))) {
    lastErrorText = job.lifecycleMetadata.value(QStringLiteral("lastError")).toString();
  } else if (job.legacyMetadata.contains(QStringLiteral("lastError"))) {
    lastErrorText = job.legacyMetadata.value(QStringLiteral("lastError")).toString();
  } else if (m_errorsModel) {
    for (int i = 0; i < m_errorsModel->rowCount(); ++i) {
      QModelIndex idx = m_errorsModel->index(i, 0);
      if (m_errorsModel->data(idx, ErrorsModel::JobIdRole).toString() == job.id) {
        lastErrorText = m_errorsModel->data(idx, ErrorsModel::MessageRole).toString();
        break;
      }
    }
  }

  QLabel *statusHeader = new QLabel(i18n("<b>Job Status & History:</b>"), m_zeroAttemptWidget);
  zeroLayout->addWidget(statusHeader);

  QLabel *schedLabel = new QLabel(i18n("• Scheduling: %1", schedState), m_zeroAttemptWidget);
  schedLabel->setObjectName(QStringLiteral("zeroSchedulingStateLabel"));
  zeroLayout->addWidget(schedLabel);

  QLabel *reasonLabel = new QLabel(i18n("• Jules Session Status: %1", reasonNoSession), m_zeroAttemptWidget);
  reasonLabel->setObjectName(QStringLiteral("zeroReasonLabel"));
  zeroLayout->addWidget(reasonLabel);

  if (!lastErrorText.isEmpty()) {
    QLabel *errorLabel = new QLabel(i18n("• Launch/Dispatch Error: %1", lastErrorText), m_zeroAttemptWidget);
    errorLabel->setObjectName(QStringLiteral("zeroErrorLabel"));
    errorLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    zeroLayout->addWidget(errorLabel);
  }

  QString createdStr = job.createdAt.isValid()
                           ? job.createdAt.toLocalTime().toString(QLocale::system().dateFormat(QLocale::ShortFormat))
                           : i18n("Unknown");
  QLabel *timestampsLabel = new QLabel(i18n("• Created: %1", createdStr), m_zeroAttemptWidget);
  timestampsLabel->setObjectName(QStringLiteral("zeroTimestampsLabel"));
  zeroLayout->addWidget(timestampsLabel);

  zeroLayout->addSpacing(10);

  QLabel *promptLabel = new QLabel(i18n("<b>Prompt:</b>"), m_zeroAttemptWidget);
  zeroLayout->addWidget(promptLabel);

  QTextBrowser *promptBrowser = new QTextBrowser(m_zeroAttemptWidget);
  promptBrowser->setObjectName(QStringLiteral("zeroPromptBrowser"));
  promptBrowser->setPlainText(job.canonicalRequest.value(QStringLiteral("prompt")).toString());
  zeroLayout->addWidget(promptBrowser);

  zeroLayout->addSpacing(10);

  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  QPushButton *launchButton =
      new QPushButton(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Launch Attempt"));
  launchButton->setObjectName(QStringLiteral("zeroLaunchButton"));
  connect(launchButton, &QPushButton::clicked, this, [this]() {
    Q_EMIT newAttemptRequested(m_jobId,
                               m_jobStore ? m_jobStore->getJobById(m_jobId)->canonicalRequest : currentSessionData());
  });
  if (isArchived) {
    launchButton->setEnabled(false);
    launchButton->setToolTip(i18n("Cannot launch attempts on an archived job."));
  }
  buttonsLayout->addWidget(launchButton);

  QPushButton *variantButton =
      new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Launch Variant..."));
  variantButton->setObjectName(QStringLiteral("zeroVariantButton"));
  connect(variantButton, &QPushButton::clicked, this,
          [this]() { Q_EMIT variantRequested(m_jobId, currentVariantRequest()); });
  if (isArchived) {
    variantButton->setEnabled(false);
    variantButton->setToolTip(i18n("Cannot launch variant on an archived job."));
  }
  buttonsLayout->addWidget(variantButton);
  buttonsLayout->addStretch();
  zeroLayout->addLayout(buttonsLayout);

  zeroLayout->addStretch();
}

void SessionWindow::updateAttemptList() {
  if (!m_jobStore)
    return;
  JobData *jobOpt = m_jobStore->getJobById(m_jobId);
  if (!jobOpt)
    return;

  JobData job = *jobOpt;

  if (job.attempts.size() <= 1) {
    m_attemptList->hide();
  } else {
    m_attemptList->show();
  }

  QString previousSelectedId = m_currentAttemptId;
  m_attemptList->clear();

  for (const auto &attempt : job.attempts) {
    QString title = attempt.id;
    if (job.acceptedAttemptId == attempt.id) {
      title += QStringLiteral(" [WINNER]");
    }
    title += QStringLiteral(" - ") + attempt.julesState;

    QListWidgetItem *item = new QListWidgetItem(title);
    item->setData(Qt::UserRole, attempt.id);
    m_attemptList->addItem(item);

    if (attempt.id == previousSelectedId) {
      item->setSelected(true);
      m_attemptList->setCurrentItem(item);
    }
  }

  if (m_attemptList->count() > 0 && !m_attemptList->currentItem()) {
    m_attemptList->setCurrentRow(0);
    m_currentAttemptId = m_attemptList->item(0)->data(Qt::UserRole).toString();
  }

  if (job.attempts.isEmpty()) {
    m_contentStack->setCurrentWidget(m_zeroAttemptWidget);
    renderZeroAttempts();
  } else {
    m_contentStack->setCurrentWidget(m_detailsWidget);
    renderDetailsAndDiff();
  }

  if (m_errorTab) {
    SessionErrorFilterProxyModel *proxy =
        m_errorTab->findChild<SessionErrorFilterProxyModel *>(QStringLiteral("errorProxy"));
    if (proxy) {
      if (m_jobStore) {
        proxy->setFilterTarget(m_jobId, m_currentAttemptId,
                               currentSessionData().value(QStringLiteral("id")).toString());
      } else {
        proxy->setSessionId(currentSessionData().value(QStringLiteral("id")).toString());
      }
    }
  }

  updateActionStates();
}

void SessionWindow::onAttemptSelected(QListWidgetItem *item) {
  if (!item)
    return;
  QString attemptId = item->data(Qt::UserRole).toString();
  if (attemptId != m_currentAttemptId) {
    m_currentAttemptId = attemptId;
    renderDetailsAndDiff();

    // Update the error proxy filter if it exists
    if (m_errorTab) {
      SessionErrorFilterProxyModel *proxy =
          m_errorTab->findChild<SessionErrorFilterProxyModel *>(QStringLiteral("errorProxy"));
      if (proxy) {
        if (m_jobStore) {
          proxy->setFilterTarget(m_jobId, m_currentAttemptId,
                                 currentSessionData().value(QStringLiteral("id")).toString());
        } else {
          proxy->setSessionId(currentSessionData().value(QStringLiteral("id")).toString());
        }
      }
    }

    refreshSession(false);
  }
}
