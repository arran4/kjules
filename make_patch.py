import re

with open('src/mainwindow.h', 'r') as f:
    header = f.read()
header = header.replace('void updateTabTitles();', 'void updateTabTitles();\n  void updateSelectionDependentActions();')
with open('src/mainwindow.h', 'w') as f:
    f.write(header)


with open('src/mainwindow.cpp', 'r') as f:
    cpp = f.read()

# Add updateSelectionDependentActions
func = """void MainWindow::updateSelectionDependentActions() {
  if (!m_tabWidget || !m_tabWidget->currentWidget())
    return;

  bool hasSelection = false;
  if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("sourcesTab")) {
    hasSelection = m_sourceView->selectionModel()->hasSelection();
  } else if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab")) {
    hasSelection = m_sessionView->selectionModel()->hasSelection();
  }

  if (m_openCurrentUrlAction) {
    if (hasSelection) {
      m_openCurrentUrlAction->setText(i18n("Open URLs for selected"));
    } else {
      m_openCurrentUrlAction->setText(i18n("Open all URLs"));
    }
  }

  if (m_refreshCurrentTabAction) {
    if (hasSelection) {
      m_refreshCurrentTabAction->setText(i18n("Refresh Selected"));
    } else {
      m_refreshCurrentTabAction->setText(i18n("Refresh"));
    }
  }
}

void MainWindow::updateTabTitles() {"""
cpp = cpp.replace('void MainWindow::updateTabTitles() {', func)

# Connect signals
conn = """  connect(m_errorsFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
    if (auto *pm = qobject_cast<QSortFilterProxyModel *>(m_errorsView->model())) {
      pm->setFilterCaseSensitivity(Qt::CaseInsensitive);
      pm->setFilterFixedString(text);
    }
  });

  connect(m_sessionView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateSelectionDependentActions);
  connect(m_sourceView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateSelectionDependentActions);
  connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateSelectionDependentActions);
}"""
cpp = cpp.replace("""  connect(m_errorsFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
    if (auto *pm = qobject_cast<QSortFilterProxyModel *>(m_errorsView->model())) {
      pm->setFilterCaseSensitivity(Qt::CaseInsensitive);
      pm->setFilterFixedString(text);
    }
  });
}""", conn)

# Update refresh action
refresh_before = """  connect(m_refreshCurrentTabAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("sourcesTab")) {
        m_refreshSourcesAction->trigger();
      } else if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab")) {
        m_refreshFollowingAction->trigger();
      }
    }
  });"""
refresh_after = """  connect(m_refreshCurrentTabAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("sourcesTab")) {
        if (m_sourceView->selectionModel()->hasSelection()) {
          m_refreshSourceAction->trigger();
        } else {
          m_refreshSourcesAction->trigger();
        }
      } else if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab")) {
        if (m_sessionView->selectionModel()->hasSelection()) {
          QModelIndexList selectedRows = m_sessionView->selectionModel()->selectedRows();
          QStringList idsToRefresh;
          const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
          for (const QModelIndex &idx : selectedRows) {
            QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
            QString id = m_sessionModel->data(mappedIdx, SessionModel::IdRole).toString();
            if (!id.isEmpty()) idsToRefresh.append(id);
          }
          if (!idsToRefresh.isEmpty()) {
            if (m_refreshProgressWindow && !m_refreshProgressWindow->isFinishedProcess()) {
              m_refreshProgressWindow->addSessionIds(idsToRefresh);
              m_refreshProgressWindow->show();
              m_refreshProgressWindow->raise();
              m_refreshProgressWindow->activateWindow();
            } else {
              if (m_refreshProgressWindow) {
                m_refreshProgressWindow->deleteLater();
              }
              m_refreshProgressWindow = new RefreshProgressWindow(idsToRefresh, m_apiManager, m_sessionModel, this);
              connect(m_refreshProgressWindow, &RefreshProgressWindow::progressUpdated, this,
                      &MainWindow::onRefreshProgressUpdated);
              connect(m_refreshProgressWindow, &RefreshProgressWindow::progressFinished, this,
                      &MainWindow::onRefreshProgressFinished);
              connect(m_refreshProgressWindow, &RefreshProgressWindow::openSessionRequested, this, [this](const QString &id) {
                m_apiManager->getSession(id);
                updateStatus(i18n("Fetching details for session %1...", id));
              });
              m_refreshProgressWindow->show();
            }
          }
        } else {
          m_refreshFollowingAction->trigger();
        }
      }
    }
  });"""
cpp = cpp.replace(refresh_before, refresh_after)

# Update m_openCurrentUrlAction
open_url_before = """  m_openCurrentUrlAction =
      new QAction(QIcon::fromTheme(QStringLiteral("internet-web-browser")), i18n("Open URLs for selected"), this);
  actionCollection()->addAction(QStringLiteral("open_current_url"), m_openCurrentUrlAction);
  connect(m_openCurrentUrlAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("sourcesTab")) {
        m_openUrlAction->trigger();
      } else if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab")) {
        QModelIndexList selectedRows = m_sessionView->selectionModel()->selectedRows();
        if (selectedRows.isEmpty())
          return;
        const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());

        int count = 0;
        for (const QModelIndex &idx : selectedRows) {"""
open_url_after = """  m_openCurrentUrlAction =
      new QAction(QIcon::fromTheme(QStringLiteral("internet-web-browser")), i18n("Open URLs for selected"), this);
  actionCollection()->addAction(QStringLiteral("open_current_url"), m_openCurrentUrlAction);
  actionCollection()->setDefaultShortcut(m_openCurrentUrlAction, QKeySequence(Qt::CTRL | Qt::Key_O));
  connect(m_openCurrentUrlAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("sourcesTab")) {
        m_openUrlAction->trigger();
      } else if (m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab")) {
        QModelIndexList selectedRows = m_sessionView->selectionModel()->selectedRows();
        const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());

        QList<QModelIndex> itemsToOpen;
        if (!selectedRows.isEmpty()) {
          itemsToOpen = selectedRows;
        } else {
          auto model = m_sessionView->model();
          if (model) {
            for (int i = 0; i < model->rowCount(); ++i) {
              itemsToOpen.append(model->index(i, 0));
            }
          }
        }

        int count = 0;
        for (const QModelIndex &idx : itemsToOpen) {"""
cpp = cpp.replace(open_url_before, open_url_after)

# Update m_openCurrentUrlAction status message
open_url_status_before = """        if (count > 0) {
          updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
        } else {
          updateStatus(i18n("No URLs found for selected sessions."));
        }"""
open_url_status_after = """        if (count > 0) {
          updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
        } else {
          updateStatus(selectedRows.isEmpty() ? i18n("No URLs found.") : i18n("No URLs found for selected sessions."));
        }"""
cpp = cpp.replace(open_url_status_before, open_url_status_after)

# Smart menu
smart_menu_before = """  QMenu *openMenu = new QMenu(this);
  m_openCurrentUrlAction->setMenu(openMenu);

  QMenu *githubMenu = new QMenu(i18n("Open all Github URLs"), this);
  m_openAllGithubUrlsAction = new QAction(i18n("All"), this);
  m_openAllGithubInProgressAction = new QAction(i18n("In Progress"), this);
  m_openAllGithubCompleteAction = new QAction(i18n("Complete"), this);
  m_openAllGithubWaitingFeedbackAction = new QAction(i18n("Waiting Feedback"), this);
  githubMenu->addAction(m_openAllGithubUrlsAction);
  githubMenu->addAction(m_openAllGithubInProgressAction);
  githubMenu->addAction(m_openAllGithubCompleteAction);
  githubMenu->addAction(m_openAllGithubWaitingFeedbackAction);

  QMenu *julesMenu = new QMenu(i18n("Open all Jules URLs"), this);
  m_openAllJulesUrlsAction = new QAction(i18n("All"), this);
  m_openAllJulesInProgressAction = new QAction(i18n("In Progress"), this);
  m_openAllJulesCompleteAction = new QAction(i18n("Complete"), this);
  m_openAllJulesWaitingFeedbackAction = new QAction(i18n("Waiting Feedback"), this);
  julesMenu->addAction(m_openAllJulesUrlsAction);
  julesMenu->addAction(m_openAllJulesInProgressAction);
  julesMenu->addAction(m_openAllJulesCompleteAction);
  julesMenu->addAction(m_openAllJulesWaitingFeedbackAction);

  QMenu *julesNoGithubMenu = new QMenu(i18n("Open all Jules URLs (no Github URL)"), this);
  m_openAllJulesNoGithubUrlsAction = new QAction(i18n("All"), this);
  m_openAllJulesNoGithubInProgressAction = new QAction(i18n("In Progress"), this);
  m_openAllJulesNoGithubCompleteAction = new QAction(i18n("Complete"), this);
  m_openAllJulesNoGithubWaitingFeedbackAction = new QAction(i18n("Waiting Feedback"), this);
  julesNoGithubMenu->addAction(m_openAllJulesNoGithubUrlsAction);
  julesNoGithubMenu->addAction(m_openAllJulesNoGithubInProgressAction);
  julesNoGithubMenu->addAction(m_openAllJulesNoGithubCompleteAction);
  julesNoGithubMenu->addAction(m_openAllJulesNoGithubWaitingFeedbackAction);

  openMenu->addMenu(githubMenu);
  openMenu->addMenu(julesMenu);
  openMenu->addMenu(julesNoGithubMenu);

  connect(openMenu, &QMenu::aboutToShow, this, [this, openMenu, githubMenu, julesMenu, julesNoGithubMenu]() {
    bool isFollowingTab = (m_tabWidget && m_tabWidget->currentWidget() &&
                           m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab"));

    githubMenu->menuAction()->setVisible(isFollowingTab);
    julesMenu->menuAction()->setVisible(isFollowingTab);
    julesNoGithubMenu->menuAction()->setVisible(isFollowingTab);

    if (!isFollowingTab) {
      openMenu->hide();
    }
  });

  auto openUrlsByStateAndType = [this](const QString &state, const QString &type) {
    int count = 0;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
      QModelIndex index = m_sessionModel->index(i, 0);
      QString currentState = m_sessionModel->data(index, SessionModel::StateRole).toString();

      if (state.isEmpty() || currentState == state) {
        QString prUrl = m_sessionModel->data(index, SessionModel::PrUrlRole).toString();
        QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();

        if (type == QStringLiteral("github") && !prUrl.isEmpty()) {
          Utils::openUrl(QUrl(prUrl));
          count++;
        } else if (type == QStringLiteral("jules") && !id.isEmpty()) {
          Utils::openUrl(QUrl(QStringLiteral("https://jules.google.com/session/") + id));
          count++;
        } else if (type == QStringLiteral("jules_no_github") && prUrl.isEmpty() && !id.isEmpty()) {
          Utils::openUrl(QUrl(QStringLiteral("https://jules.google.com/session/") + id));
          count++;
        }
      }
    }
    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(i18n("No URLs found matching criteria."));
    }
  };

  connect(m_openAllGithubUrlsAction, &QAction::triggered, this,
          [openUrlsByStateAndType]() { openUrlsByStateAndType(QString(), QStringLiteral("github")); });
  connect(m_openAllGithubInProgressAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("IN_PROGRESS"), QStringLiteral("github"));
  });
  connect(m_openAllGithubCompleteAction, &QAction::triggered, this,
          [openUrlsByStateAndType]() { openUrlsByStateAndType(QStringLiteral("DONE"), QStringLiteral("github")); });
  connect(m_openAllGithubWaitingFeedbackAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("WAITING_FEEDBACK"), QStringLiteral("github"));
  });

  connect(m_openAllJulesUrlsAction, &QAction::triggered, this,
          [openUrlsByStateAndType]() { openUrlsByStateAndType(QString(), QStringLiteral("jules")); });
  connect(m_openAllJulesInProgressAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("IN_PROGRESS"), QStringLiteral("jules"));
  });
  connect(m_openAllJulesCompleteAction, &QAction::triggered, this,
          [openUrlsByStateAndType]() { openUrlsByStateAndType(QStringLiteral("DONE"), QStringLiteral("jules")); });
  connect(m_openAllJulesWaitingFeedbackAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("WAITING_FEEDBACK"), QStringLiteral("jules"));
  });

  connect(m_openAllJulesNoGithubUrlsAction, &QAction::triggered, this,
          [openUrlsByStateAndType]() { openUrlsByStateAndType(QString(), QStringLiteral("jules_no_github")); });
  connect(m_openAllJulesNoGithubInProgressAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("IN_PROGRESS"), QStringLiteral("jules_no_github"));
  });
  connect(m_openAllJulesNoGithubCompleteAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("DONE"), QStringLiteral("jules_no_github"));
  });
  connect(m_openAllJulesNoGithubWaitingFeedbackAction, &QAction::triggered, this, [openUrlsByStateAndType]() {
    openUrlsByStateAndType(QStringLiteral("WAITING_FEEDBACK"), QStringLiteral("jules_no_github"));
  });"""
smart_menu_after = """  QMenu *openMenu = new QMenu(this);
  m_openCurrentUrlAction->setMenu(openMenu);

  QAction *openAllAction = openMenu->addAction(i18n("All"));
  QAction *openInProgressAction = openMenu->addAction(i18n("In Progress"));
  QAction *openCompleteAction = openMenu->addAction(i18n("Complete"));
  QAction *openWaitingFeedbackAction = openMenu->addAction(i18n("Waiting Feedback"));
  QAction *openAllButInProgressAction = openMenu->addAction(i18n("All but In Progress"));
  QAction *openAllButCompleteAction = openMenu->addAction(i18n("All but Complete"));

  connect(openMenu, &QMenu::aboutToShow, this, [this, openMenu]() {
    bool isFollowingTab = (m_tabWidget && m_tabWidget->currentWidget() &&
                           m_tabWidget->currentWidget()->objectName() == QStringLiteral("followingTab"));
    if (!isFollowingTab) {
      openMenu->hide();
    }
  });

  auto openUrlsByStateFilter = [this](std::function<bool(const QString&)> stateFilter) {
    int count = 0;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
      QModelIndex index = m_sessionModel->index(i, 0);
      QString currentState = m_sessionModel->data(index, SessionModel::StateRole).toString();

      if (stateFilter(currentState)) {
        QString prUrl = m_sessionModel->data(index, SessionModel::PrUrlRole).toString();

        if (!prUrl.isEmpty()) {
          Utils::openUrl(QUrl(prUrl));
          count++;
        } else {
          QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();
          if (!id.isEmpty()) {
            Utils::openUrl(QUrl(QStringLiteral("https://jules.google.com/session/") + id));
            count++;
          }
        }
      }
    }
    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(i18n("No URLs found matching criteria."));
    }
  };

  connect(openAllAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString&) { return true; }); });
  connect(openInProgressAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString& s) { return s == QStringLiteral("IN_PROGRESS"); }); });
  connect(openCompleteAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString& s) { return s == QStringLiteral("DONE"); }); });
  connect(openWaitingFeedbackAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString& s) { return s == QStringLiteral("WAITING_FEEDBACK"); }); });
  connect(openAllButInProgressAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString& s) { return s != QStringLiteral("IN_PROGRESS"); }); });
  connect(openAllButCompleteAction, &QAction::triggered, this,
          [openUrlsByStateFilter]() { openUrlsByStateFilter([](const QString& s) { return s != QStringLiteral("DONE"); }); });"""
cpp = cpp.replace(smart_menu_before, smart_menu_after)

# Update m_openUrlAction for sources
open_url_sources_before = """  m_openUrlAction = new QAction(i18n("Open URL"), this);
  actionCollection()->addAction(QStringLiteral("open_url"), m_openUrlAction);
  connect(m_openUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows = m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {"""
open_url_sources_after = """  m_openUrlAction = new QAction(i18n("Open URL"), this);
  actionCollection()->addAction(QStringLiteral("open_url"), m_openUrlAction);
  connect(m_openUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows = m_sourceView->selectionModel()->selectedRows();
    const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    QList<QModelIndex> itemsToOpen;
    if (!selectedRows.isEmpty()) {
      itemsToOpen = selectedRows;
    } else {
      auto model = m_sourceView->model();
      if (model) {
        for (int i = 0; i < model->rowCount(); ++i) {
          itemsToOpen.append(model->index(i, 0));
        }
      }
    }

    int count = 0;
    for (const QModelIndex &idx : itemsToOpen) {"""
cpp = cpp.replace(open_url_sources_before, open_url_sources_after)

open_url_sources_status_before = """    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(i18n("Invalid source ID for opening URL."));
    }"""
open_url_sources_status_after = """    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(selectedRows.isEmpty() ? i18n("No URLs found.") : i18n("Invalid source ID for opening URL."));
    }"""
cpp = cpp.replace(open_url_sources_status_before, open_url_sources_status_after)

# Ensure std::function is available
if "#include <functional>" not in cpp:
    cpp = "#include <functional>\n" + cpp

with open('src/mainwindow.cpp', 'w') as f:
    f.write(cpp)
