import re

with open("src/sessionwindow.cpp", "r") as f:
    content = f.read()

new_methods = """
void SessionWindow::renderZeroAttempts() {
  if (!m_jobStore) return;
  JobData *jobOpt = m_jobStore->getJobById(m_jobId);
  if (!jobOpt) return;

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

  QLabel *titleLabel = new QLabel(i18n("<b>Job:</b> %1", job.canonicalRequest.value(QStringLiteral("title")).toString()), m_zeroAttemptWidget);
  zeroLayout->addWidget(titleLabel);

  QLabel *idLabel = new QLabel(i18n("<b>ID:</b> %1", job.id), m_zeroAttemptWidget);
  zeroLayout->addWidget(idLabel);

  QJsonObject sourceContext = job.canonicalRequest.value(QStringLiteral("sourceContext")).toObject();
  QString source = sourceContext.value(QStringLiteral("source")).toString();
  QLabel *sourceLabel = new QLabel(i18n("<b>Source:</b> %1", source), m_zeroAttemptWidget);
  zeroLayout->addWidget(sourceLabel);

  QString branch = sourceContext.value(QStringLiteral("githubRepoContext")).toObject().value(QStringLiteral("startingBranch")).toString();
  if (!branch.isEmpty()) {
      QLabel *branchLabel = new QLabel(i18n("<b>Branch:</b> %1", branch), m_zeroAttemptWidget);
      zeroLayout->addWidget(branchLabel);
  }

  QLabel *statusLabel = new QLabel(i18n("<b>Status:</b> No remote Jules sessions (attempts) have been made yet."), m_zeroAttemptWidget);
  zeroLayout->addWidget(statusLabel);

  zeroLayout->addSpacing(10);

  QLabel *promptLabel = new QLabel(i18n("<b>Prompt:</b>"), m_zeroAttemptWidget);
  zeroLayout->addWidget(promptLabel);

  QTextBrowser *promptBrowser = new QTextBrowser(m_zeroAttemptWidget);
  promptBrowser->setPlainText(job.canonicalRequest.value(QStringLiteral("prompt")).toString());
  zeroLayout->addWidget(promptBrowser);

  zeroLayout->addStretch();
}

void SessionWindow::updateAttemptList() {
  if (!m_jobStore) return;
  JobData *jobOpt = m_jobStore->getJobById(m_jobId);
  if (!jobOpt) return;

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
}

void SessionWindow::onAttemptSelected(QListWidgetItem *item) {
  if (!item) return;
  QString attemptId = item->data(Qt::UserRole).toString();
  if (attemptId != m_currentAttemptId) {
    m_currentAttemptId = attemptId;
    renderDetailsAndDiff();
    refreshSession(false);
  }
}
"""

content += new_methods

with open("src/sessionwindow.cpp", "w") as f:
    f.write(content)
