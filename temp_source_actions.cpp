void MainWindow::createSourceActions() {
  m_sourceSettingsAction = new QAction(i18n("Source Settings"), this);
  actionCollection()->addAction(QStringLiteral("source_settings"),
                                m_sourceSettingsAction);
  connect(m_sourceSettingsAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    if (selectedRows.size() > 0) {
      QModelIndex idx = selectedRows.first();
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QMainWindow *settingsWindow = new QMainWindow(this);
      settingsWindow->setObjectName(
          QStringLiteral("SourceSettingsWindow_%1")
              .arg(m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                       .toString()
                       .replace(QLatin1Char('/'), QLatin1Char('_'))));
      settingsWindow->setAttribute(Qt::WA_DeleteOnClose);
      settingsWindow->setWindowTitle(i18n("Source Settings"));

      QTextEdit *textEdit = new QTextEdit(settingsWindow);
      QJsonObject rawData =
          m_sourceModel->data(mappedIdx, SourceModel::RawDataRole)
              .toJsonObject();
      QJsonDocument doc(rawData);
      textEdit->setPlainText(
          QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

      QMenu *fileMenu = new QMenu(i18n("File"), settingsWindow);
      QAction *saveAction =
          new QAction(QIcon::fromTheme(QStringLiteral("document-save")),
                      i18n("Save"), settingsWindow);
      saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
      connect(saveAction, &QAction::triggered, settingsWindow,
              [this, textEdit, mappedIdx, settingsWindow]() {
                QJsonParseError parseError;
                QJsonDocument newDoc = QJsonDocument::fromJson(
                    textEdit->toPlainText().toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError) {
                  QMessageBox::warning(settingsWindow, i18n("Invalid JSON"),
                                       i18n("The JSON data is invalid: %1",
                                            parseError.errorString()));
                  return;
                }
                QJsonObject mergedData = newDoc.object();

                m_sourceModel->updateSource(mergedData);
                updateStatus(i18n("Source settings saved successfully."));
              });
      fileMenu->addAction(saveAction);

      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), settingsWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, settingsWindow,
              &QMainWindow::close);
      fileMenu->addAction(closeAction);
      settingsWindow->menuBar()->addMenu(fileMenu);

      settingsWindow->setCentralWidget(textEdit);
      settingsWindow->resize(600, 400);
      settingsWindow->show();
    }
  });

  m_refreshSourcesAction =
      new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                  i18n("Refresh Sources"), this);
  connect(m_refreshSourcesAction, &QAction::triggered, this,
          &MainWindow::refreshSources);
  actionCollection()->addAction(QStringLiteral("refresh_sources"),
                                m_refreshSourcesAction);

  QAction *refreshCurrentTabAction =
      new QAction(i18n("Refresh Current Tab"), this);
  actionCollection()->setDefaultShortcut(refreshCurrentTabAction,
                                         QKeySequence(Qt::Key_F5));
  connect(refreshCurrentTabAction, &QAction::triggered, this, [this]() {
    if (m_tabWidget && m_tabWidget->currentWidget()) {
      if (m_tabWidget->currentWidget()->objectName() ==
          QStringLiteral("sourcesTab")) {
        m_refreshSourcesAction->trigger();
      } else if (m_tabWidget->currentWidget()->objectName() ==
                 QStringLiteral("followingTab")) {
        m_refreshFollowingAction->trigger();
      }
    }
  });
  actionCollection()->addAction(QStringLiteral("refresh_current_tab"),
                                refreshCurrentTabAction);

  m_refreshSourceAction = new QAction(i18n("Refresh Source"), this);
  actionCollection()->addAction(QStringLiteral("refresh_source"),
                                m_refreshSourceAction);

  m_recalculateStatsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("tools-wizard")),
                  i18n("Recalculate Session Stats"), this);
  actionCollection()->addAction(QStringLiteral("recalculate_stats"),
                                m_recalculateStatsAction);
  connect(m_recalculateStatsAction, &QAction::triggered, this, [this]() {
    QJsonArray allSessions;
    QJsonArray active = m_sessionModel->getAllSessions();
    for (int i = 0; i < active.size(); ++i) {
      allSessions.append(active[i]);
    }
    QJsonArray archived = m_archiveModel->getAllSessions();
    for (int i = 0; i < archived.size(); ++i) {
      allSessions.append(archived[i]);
    }
    m_sourceModel->recalculateStatsFromSessions(allSessions);
    updateStatus(i18n("Session statistics recalculated successfully."));
  });
  connect(m_refreshSourceAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();
      m_apiManager->getSource(id);
      count++;
    }
    updateStatus(
        i18np("Refreshing 1 source...", "Refreshing %1 sources...", count));
  });
  m_showFollowingNewSessionsAction =
      new QAction(i18n("Show following new sessions"), this);
  actionCollection()->addAction(QStringLiteral("show_following_new_sessions"),
                                m_showFollowingNewSessionsAction);
  connect(
      m_showFollowingNewSessionsAction, &QAction::triggered, this, [this]() {
        QModelIndexList selectedRows =
            m_sourceView->selectionModel()->selectedRows();
        if (selectedRows.isEmpty())
          return;
        const QSortFilterProxyModel *proxy =
            qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

        QString path =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QFile file(path + QStringLiteral("/cached_sessions.json"));
        QJsonArray cachedSessions;
        if (file.open(QIODevice::ReadOnly)) {
          QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
          cachedSessions = doc.array();
          file.close();
        }

        for (const QModelIndex &idx : selectedRows) {
          QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
          QString id =
              m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();
          QJsonArray filteredSessions;
          for (int i = 0; i < cachedSessions.size(); ++i) {
            QJsonObject session = cachedSessions[i].toObject();
            QString sessionSource =
                session.value(QStringLiteral("sourceContext"))
                    .toObject()
                    .value(QStringLiteral("source"))
                    .toString();
            if (sessionSource == id) {
              filteredSessions.append(session);
            }
          }
          QMainWindow *sessionsWindow = new QMainWindow(this);
          sessionsWindow->setObjectName(
              QStringLiteral("FollowingNewSessions_%1").arg(id));
          SessionModel *localModel = new SessionModel(
              QStringLiteral("cached_all_sessions.json"), sessionsWindow);
          localModel->setSessions(filteredSessions);
          sessionsWindow->setAttribute(Qt::WA_DeleteOnClose);
          sessionsWindow->setWindowTitle(
              i18n("Following New Sessions for %1", id));
          QListView *listView = new QListView(sessionsWindow);
          listView->setModel(localModel);
          listView->setItemDelegate(new SessionDelegate(listView));
          connect(listView, &QListView::doubleClicked, this,
                  [this, localModel](const QModelIndex &filterIndex) {
                    QString sessId =
                        localModel->data(filterIndex, SessionModel::IdRole)
                            .toString();
                    m_apiManager->getSession(sessId);
                    updateStatus(
                        i18n("Fetching details for session %1...", sessId));
                  });
          QMenu *fileMenu = new QMenu(i18n("File"), sessionsWindow);
          QAction *closeAction =
              new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                          i18n("Close"), sessionsWindow);
          closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
          connect(closeAction, &QAction::triggered, sessionsWindow,
                  &QMainWindow::close);
          fileMenu->addAction(closeAction);
          sessionsWindow->menuBar()->addMenu(fileMenu);

          sessionsWindow->setCentralWidget(listView);
          sessionsWindow->resize(600, 400);
          sessionsWindow->show();
        }
      });
  m_viewRawDataAction = new QAction(i18n("View Raw Data"), this);
  actionCollection()->addAction(QStringLiteral("view_raw_data"),
                                m_viewRawDataAction);
  connect(m_viewRawDataAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QJsonObject rawData =
          m_sourceModel->data(mappedIdx, SourceModel::RawDataRole)
              .toJsonObject();
      QMainWindow *rawWindow = new QMainWindow(this);
      rawWindow->setObjectName(
          QStringLiteral("RawDataWindow_%1")
              .arg(m_sourceModel->data(mappedIdx, SourceModel::IdRole)
                       .toString()
                       .replace(QLatin1Char('/'), QLatin1Char('_'))));
      rawWindow->setAttribute(Qt::WA_DeleteOnClose);
      rawWindow->setWindowTitle(i18n("Raw Data for Source"));
      QTabWidget *tabWidget = new QTabWidget(rawWindow);

      QTextBrowser *sourceBrowser = new QTextBrowser(tabWidget);
      QJsonDocument doc(rawData);
      sourceBrowser->setPlainText(
          QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
      tabWidget->addTab(sourceBrowser, i18n("Source Data"));

      if (rawData.contains(QStringLiteral("github"))) {
        QTextBrowser *githubBrowser = new QTextBrowser(tabWidget);
        QJsonDocument githubDoc(
            rawData.value(QStringLiteral("github")).toObject());
        githubBrowser->setPlainText(
            QString::fromUtf8(githubDoc.toJson(QJsonDocument::Indented)));
        tabWidget->addTab(githubBrowser, i18n("GitHub API Data"));
      }

      QMenu *fileMenu = new QMenu(i18n("File"), rawWindow);
      QAction *closeAction =
          new QAction(QIcon::fromTheme(QStringLiteral("window-close")),
                      i18n("Close"), rawWindow);
      closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
      connect(closeAction, &QAction::triggered, rawWindow, &QMainWindow::close);
      fileMenu->addAction(closeAction);
      rawWindow->menuBar()->addMenu(fileMenu);

      rawWindow->setCentralWidget(tabWidget);
      rawWindow->resize(600, 400);
      rawWindow->show();
    }
  });

  m_openUrlAction = new QAction(i18n("Open URL"), this);
  actionCollection()->addAction(QStringLiteral("open_url"), m_openUrlAction);
  connect(m_openUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    int count = 0;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

      QString urlStr = urlFromSourceId(id);

      if (!urlStr.isEmpty()) {
        QDesktopServices::openUrl(QUrl(urlStr));
        count++;
      }
    }
    if (count > 0) {
      updateStatus(i18np("Opened 1 URL.", "Opened %1 URLs.", count));
    } else {
      updateStatus(i18n("Invalid source ID for opening URL."));
    }
  });

  m_copyUrlAction = new QAction(i18n("Copy URL"), this);
  actionCollection()->addAction(QStringLiteral("copy_url"), m_copyUrlAction);
  connect(m_copyUrlAction, &QAction::triggered, this, [this]() {
    QModelIndexList selectedRows =
        m_sourceView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
      return;
    const QSortFilterProxyModel *proxy =
        qobject_cast<const QSortFilterProxyModel *>(m_sourceView->model());

    QStringList urlsToCopy;
    for (const QModelIndex &idx : selectedRows) {
      QModelIndex mappedIdx = proxy ? proxy->mapToSource(idx) : idx;
      QString id =
          m_sourceModel->data(mappedIdx, SourceModel::IdRole).toString();

      QString urlStr = urlFromSourceId(id);

      if (!urlStr.isEmpty()) {
        urlsToCopy.append(urlStr);
      }
    }

    if (!urlsToCopy.isEmpty()) {
      QGuiApplication::clipboard()->setText(urlsToCopy.join(QLatin1Char('\n')));
      updateStatus(i18np("1 URL copied to clipboard.",
                         "%1 URLs copied to clipboard.", urlsToCopy.size()));
    } else {
      updateStatus(i18n("Invalid source ID for copying URL."));
    }
  });

  // Set up XML GUI
}
