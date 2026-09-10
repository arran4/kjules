import re

with open("src/sessionwindow.cpp", "r") as f:
    content = f.read()

setup_ui_impl = """void SessionWindow::setupUi(const QJsonObject &sessionData) {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  mainLayout->addWidget(m_splitter);

  m_attemptList = new QListWidget(this);
  m_splitter->addWidget(m_attemptList);

  connect(m_attemptList, &QListWidget::itemClicked, this, &SessionWindow::onAttemptSelected);

  m_contentStack = new QStackedWidget(this);
  m_splitter->addWidget(m_contentStack);

  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 4);

  m_zeroAttemptWidget = new QWidget(this);
  QVBoxLayout *zeroLayout = new QVBoxLayout(m_zeroAttemptWidget);
  QLabel *zeroLabel = new QLabel(i18n("No attempts have been made yet."), this);
  zeroLayout->addWidget(zeroLabel);
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
      title = m_sessionData.value(QStringLiteral("title")).toString();
      sessionId = m_sessionData.value(QStringLiteral("id")).toString();
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
}"""

content = re.sub(r'void SessionWindow::setupUi\(\) \{.*?\n\}', setup_ui_impl, content, flags=re.DOTALL)

with open("src/sessionwindow.cpp", "w") as f:
    f.write(content)
