1. **Add `followRequested` signal to `SessionsWindow`**
   - Add a signal `void followRequested(const QJsonObject &session);` in `src/sessionswindow.h`

2. **Add "Follow" action to context menu in `SessionsWindow`**
   - In `src/sessionswindow.cpp`, in the `customContextMenuRequested` handler, check if there are sessions not currently in the managed model.
   - Wait, `SessionsWindow` doesn't know about `MainWindow::m_sessionModel`.
   - I can pass a `std::function` or a signal to MainWindow.
   - Since `MainWindow` creates `SessionsWindow`, we can connect a signal from `SessionsWindow` to a lambda in `MainWindow`.
   - To know if a session is already followed, we need to check if the session is in `MainWindow`'s `m_sessionModel`.
   - Let's expose `hasSession(const QString &id)` in `SessionModel`.

3. **Update `SessionModel` with `hasSession` method**
   - In `src/sessionmodel.h`, add `bool hasSession(const QString &id) const;`
   - In `src/sessionmodel.cpp`, implement it to return `m_idToIndex.contains(id);`

4. **Add `hasSession` to `SessionsWindow` constructor or provide a callback**
   - Better yet, when `SessionsWindow` constructs its context menu, we could ask a delegate or emit a signal, or we can just pass a `std::function<bool(const QString&)> isFollowingCallback` to `SessionsWindow`'s constructor. Or we can just emit `followRequested` for any session. Let's see. If the session is already followed, maybe the context menu should show "Unfollow" or just hide "Follow"? The user request: "an option I should have on any I am not already "following" is to "Follow" which means is to adopt it as a managed kjules session."
   - So we need a way to check if a session is already managed.

5. **Modify `SessionsWindow` constructor to accept a `SessionModel *managedModel`?**
   - The user currently opens "All sessions" from `MainWindow`.
   - `MainWindow` creates `SessionsWindow`: `SessionsWindow *window = new SessionsWindow(QString(), m_apiManager, this);`
   - We can add a method to `SessionsWindow`: `void setManagedSessionModel(SessionModel *model);` or we could pass `SessionModel *` in constructor. Wait, `SessionsWindow` also manages its own `m_model` which is a `SessionModel`.
   - In `src/sessionswindow.h`, add `SessionModel *m_managedModel = nullptr;`
   - In `src/sessionswindow.h`, add `void setManagedModel(SessionModel *model) { m_managedModel = model; }`
   - In `src/mainwindow.cpp`, after creating `SessionsWindow`, call `window->setManagedModel(m_sessionModel);`
   - In `src/sessionswindow.cpp`, context menu:
     ```cpp
     bool hasUnfollowed = false;
     for (const QModelIndex &idx : selectedRows) {
       QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
       if (m_managedModel && !m_managedModel->hasSession(id)) {
         hasUnfollowed = true;
         break;
       }
     }

     if (hasUnfollowed) {
       menu.addSeparator();
       QAction *followAction = menu.addAction(i18n("Follow"));
       connect(followAction, &QAction::triggered, [this, selectedRows]() {
         for (const QModelIndex &idx : selectedRows) {
           QString id = m_proxyModel->data(idx, SessionModel::IdRole).toString();
           if (m_managedModel && !m_managedModel->hasSession(id)) {
             QModelIndex sourceIndex = m_proxyModel->mapToSource(idx);
             QJsonObject rawData = m_model->getSession(sourceIndex.row());
             m_managedModel->addSession(rawData);
           }
         }
         if (m_managedModel) {
           m_managedModel->saveSessions();
         }
         m_statusLabel->setText(i18n("Started following selected sessions."));
       });
     }
     ```

6. **Pre-commit step**
   - Run cmake, ctest, etc.
