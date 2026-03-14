import re

with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

# Replace m_sessionView creation
session_view_old = """  // Sessions View
  m_sessionView = new QListView(this);
  m_sessionView->setModel(m_sessionModel);
  m_sessionView->setItemDelegate(new SessionDelegate(this));"""
session_view_new = """  // Sessions View
  m_sessionView = new QTreeView(this);
  QSortFilterProxyModel *sessionProxyModel = new QSortFilterProxyModel(this);
  sessionProxyModel->setSourceModel(m_sessionModel);
  sessionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  sessionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_sessionView->setModel(sessionProxyModel);
  m_sessionView->setSortingEnabled(true);
  m_sessionView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sessionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_sessionView->header()->setStretchLastSection(true);"""
content = content.replace(session_view_old, session_view_new)

# Replace session view context menu
session_ctx_old = """      m_sessionView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *viewSessionsAction = menu.addAction(i18n("View Sessions"));
          QAction *openUrlAction = menu.addAction(i18n("Open URL"));
          QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

          connect(viewSessionsAction, &QAction::triggered, [this, index]() {
            QString source =
                m_sessionModel->data(index, SessionModel::SourceRole).toString();
            SessionsWindow *window = new SessionsWindow(source, m_apiManager, this);
            window->show();
          });

          connect(openUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();

            QRegularExpression idRegex(QStringLiteral("^[a-zA-Z0-9_-]+$"));
            if (!idRegex.match(id).hasMatch()) {
              updateStatus(i18n("Invalid session ID format."));
              return;
            }

            // Placeholder URL logic
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();

            QRegularExpression idRegex(QStringLiteral("^[a-zA-Z0-9_-]+$"));
            if (!idRegex.match(id).hasMatch()) {
              updateStatus(i18n("Invalid session ID format."));
              return;
            }

            // Placeholder URL logic
            QGuiApplication::clipboard()->setText(
                QStringLiteral("https://jules.google.com/sessions/") + id);
            updateStatus(i18n("URL copied to clipboard."));
          });
          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QListView::doubleClicked, this,
          &MainWindow::onSessionActivated);"""

session_ctx_new = """      m_sessionView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;

          QMenu menu;
          QAction *openSessionAction = menu.addAction(i18n("Open Session"));
          QAction *openSessionsForSourceAction = menu.addAction(i18n("Open Sessions for source"));
          QAction *refreshSessionAction = menu.addAction(i18n("Refresh session details"));
          menu.addSeparator();

          QString id = m_sessionModel->data(sourceIndex, SessionModel::IdRole).toString();
          QAction *openJulesUrlAction = nullptr;
          QAction *copyJulesUrlAction = nullptr;
          if (!id.isEmpty()) {
              openJulesUrlAction = menu.addAction(i18n("Open Jules URL"));
              copyJulesUrlAction = menu.addAction(i18n("Copy Jules URL"));
          }

          QString prUrl = m_sessionModel->data(sourceIndex, SessionModel::PrUrlRole).toString();
          QAction *openGithubUrlAction = nullptr;
          QAction *copyGithubUrlAction = nullptr;
          if (!prUrl.isEmpty()) {
              openGithubUrlAction = menu.addAction(i18n("Open Github URL"));
              copyGithubUrlAction = menu.addAction(i18n("Copy Github URL"));
          }

          menu.addSeparator();
          QAction *archiveAction = menu.addAction(i18n("Archive"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));
          menu.addSeparator();
          QAction *newSessionFromSessionAction = menu.addAction(i18n("New Session From Session"));
          QAction *saveTemplateAction = menu.addAction(i18n("Save prompt as template"));

          connect(openSessionAction, &QAction::triggered, [this, index]() {
              onSessionActivated(index);
          });

          connect(openSessionsForSourceAction, &QAction::triggered, [this, sourceIndex]() {
            QString source = m_sessionModel->data(sourceIndex, SessionModel::SourceRole).toString();
            SessionsWindow *window = new SessionsWindow(source, m_apiManager, this);
            window->show();
          });

          connect(refreshSessionAction, &QAction::triggered, [this, id]() {
              m_apiManager->reloadSession(id);
              updateStatus(i18n("Refreshing session details for %1...", id));
          });

          if (openJulesUrlAction && copyJulesUrlAction) {
              connect(openJulesUrlAction, &QAction::triggered, [this, id]() {
                  QString urlStr = QStringLiteral("https://jules.google.com/sessions/") + id;
                  QDesktopServices::openUrl(QUrl(urlStr));
                  updateStatus(i18n("Opened session %1", id));
              });
              connect(copyJulesUrlAction, &QAction::triggered, [this, id]() {
                  QGuiApplication::clipboard()->setText(QStringLiteral("https://jules.google.com/sessions/") + id);
                  updateStatus(i18n("Jules URL copied to clipboard."));
              });
          }

          if (openGithubUrlAction && copyGithubUrlAction) {
              connect(openGithubUrlAction, &QAction::triggered, [this, prUrl]() {
                  QDesktopServices::openUrl(QUrl(prUrl));
                  updateStatus(i18n("Opened Github URL"));
              });
              connect(copyGithubUrlAction, &QAction::triggered, [this, prUrl]() {
                  QGuiApplication::clipboard()->setText(prUrl);
                  updateStatus(i18n("Github URL copied to clipboard."));
              });
          }

          connect(archiveAction, &QAction::triggered, [this, sourceIndex]() {
              QJsonObject session = m_sessionModel->getSession(sourceIndex.row());
              m_archiveModel->addSession(session);
              m_archiveModel->saveSessions();
              m_sessionModel->removeSession(sourceIndex.row());
              updateStatus(i18n("Session archived."));
          });

          connect(deleteAction, &QAction::triggered, [this, sourceIndex]() {
              m_sessionModel->removeSession(sourceIndex.row());
              updateStatus(i18n("Session deleted."));
          });

          connect(newSessionFromSessionAction, &QAction::triggered, [this, sourceIndex]() {
              QJsonObject session = m_sessionModel->getSession(sourceIndex.row());
              QString prompt = session.value(QStringLiteral("prompt")).toString();
              QString source = session.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
              QJsonObject initData;
              initData[QStringLiteral("prompt")] = prompt;
              if (!source.isEmpty()) {
                  QJsonArray sourcesArr;
                  sourcesArr.append(source);
                  initData[QStringLiteral("sources")] = sourcesArr;
              }
              bool hasApiKey = !m_apiManager->apiKey().isEmpty();
              NewSessionDialog dialog(m_sourceModel, hasApiKey, this);
              dialog.setInitialData(initData);
              connect(&dialog, &NewSessionDialog::createSessionRequested, this, &MainWindow::onSessionCreated);
              connect(&dialog, &NewSessionDialog::saveDraftRequested, this, &MainWindow::onDraftSaved);
              dialog.exec();
          });

          connect(saveTemplateAction, &QAction::triggered, [this]() {
              updateStatus(i18n("TODO: Save prompt as template not yet implemented."));
          });

          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QTreeView::doubleClicked, this,
          &MainWindow::onSessionActivated);"""

content = content.replace(session_ctx_old, session_ctx_new)

# Add Archive View
archive_view = """
  // Archive View
  m_archiveView = new QTreeView(this);
  QSortFilterProxyModel *archiveProxyModel = new QSortFilterProxyModel(this);
  archiveProxyModel->setSourceModel(m_archiveModel);
  archiveProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  archiveProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_archiveView->setModel(archiveProxyModel);
  m_archiveView->setSortingEnabled(true);
  m_archiveView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_archiveView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_archiveView->header()->setStretchLastSection(true);

  m_archiveView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_archiveView, &QTreeView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_archiveView->indexAt(pos);
        if (index.isValid()) {
          const QSortFilterProxyModel *proxy =
              qobject_cast<const QSortFilterProxyModel *>(m_archiveView->model());
          QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
          QMenu menu;
          QAction *openSessionAction = menu.addAction(i18n("Open Session"));
          QAction *unarchiveAction = menu.addAction(i18n("Unarchive"));
          QAction *deleteAction = menu.addAction(i18n("Delete"));
          menu.addSeparator();
          QAction *saveTemplateAction = menu.addAction(i18n("Save prompt as template"));

          connect(openSessionAction, &QAction::triggered, [this, sourceIndex]() {
              QJsonObject sessionData = m_archiveModel->getSession(sourceIndex.row());
              if (!sessionData.isEmpty()) {
                  SessionWindow *window = new SessionWindow(sessionData, m_apiManager, this);
                  connectSessionWindow(window);
                  window->show();
              } else {
                  QString id = m_archiveModel->data(sourceIndex, SessionModel::IdRole).toString();
                  m_apiManager->getSession(id);
                  updateStatus(i18n("Fetching details for session %1...", id));
              }
          });

          connect(unarchiveAction, &QAction::triggered, [this, sourceIndex]() {
              QJsonObject session = m_archiveModel->getSession(sourceIndex.row());
              m_sessionModel->addSession(session);
              m_sessionModel->saveSessions();
              m_archiveModel->removeSession(sourceIndex.row());
              updateStatus(i18n("Session unarchived."));
          });

          connect(deleteAction, &QAction::triggered, [this, sourceIndex]() {
              m_archiveModel->removeSession(sourceIndex.row());
              updateStatus(i18n("Session deleted from archive."));
          });

          connect(saveTemplateAction, &QAction::triggered, [this]() {
              updateStatus(i18n("TODO: Save prompt as template not yet implemented."));
          });

          menu.exec(m_archiveView->mapToGlobal(pos));
        }
      });
  connect(m_archiveView, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &index) {
              const QSortFilterProxyModel *proxy =
                  qobject_cast<const QSortFilterProxyModel *>(m_archiveView->model());
              QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
              QJsonObject sessionData = m_archiveModel->getSession(sourceIndex.row());
              if (!sessionData.isEmpty()) {
                  SessionWindow *window = new SessionWindow(sessionData, m_apiManager, this);
                  connectSessionWindow(window);
                  window->show();
              } else {
                  QString id = m_archiveModel->data(sourceIndex, SessionModel::IdRole).toString();
                  m_apiManager->getSession(id);
                  updateStatus(i18n("Fetching details for session %1...", id));
              }
          });

  tabWidget->addTab(m_archiveView, i18n("Archive"));
"""

content = content.replace('tabWidget->addTab(m_sessionView, i18n("Past"));', 'tabWidget->addTab(m_sessionView, i18n("Past"));' + archive_view)

# Add showSessionWindow
show_sess_old = """void MainWindow::showSessionWindow(const QJsonObject &session) {
  SessionWindow *window = new SessionWindow(session, this);
  window->show();
}

void MainWindow::onSessionActivated(const QModelIndex &index) {
  QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();
  m_apiManager->getSession(id);
  updateStatus(i18n("Fetching details for session %1...", id));
}"""

show_sess_new = """void MainWindow::connectSessionWindow(SessionWindow *window) {
  connect(window, &SessionWindow::templateRequested, this, [this](const QJsonObject &) {
    updateStatus(i18n("TODO: Save prompt as template not yet implemented."));
  });

  connect(window, &SessionWindow::archiveRequested, this, [this, window](const QString &id) {
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
      if (m_sessionModel->data(m_sessionModel->index(i, 0), SessionModel::IdRole).toString() == id) {
        QJsonObject session = m_sessionModel->getSession(i);
        m_archiveModel->addSession(session);
        m_archiveModel->saveSessions();
        m_sessionModel->removeSession(i);
        updateStatus(i18n("Session archived."));
        window->close();
        break;
      }
    }
  });

  connect(window, &SessionWindow::deleteRequested, this, [this, window](const QString &id) {
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
      if (m_sessionModel->data(m_sessionModel->index(i, 0), SessionModel::IdRole).toString() == id) {
        m_sessionModel->removeSession(i);
        updateStatus(i18n("Session deleted."));
        window->close();
        break;
      }
    }
  });
}

void MainWindow::showSessionWindow(const QJsonObject &session) {
  SessionWindow *window = new SessionWindow(session, m_apiManager, this);
  connectSessionWindow(window);
  window->show();
}

void MainWindow::onSessionActivated(const QModelIndex &index) {
  const QSortFilterProxyModel *proxy =
      qobject_cast<const QSortFilterProxyModel *>(m_sessionView->model());
  QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
  QJsonObject sessionData = m_sessionModel->getSession(sourceIndex.row());

  if (sessionData.isEmpty()) {
      QString id = m_sessionModel->data(sourceIndex, SessionModel::IdRole).toString();
      m_apiManager->getSession(id);
      updateStatus(i18n("Fetching details for session %1...", id));
  } else {
      SessionWindow *window = new SessionWindow(sessionData, m_apiManager, this);
      connectSessionWindow(window);
      window->show();
  }
}"""

content = content.replace(show_sess_old, show_sess_new)

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
