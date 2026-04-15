1. **Update `SessionModel` to support notes**
   - Add `LocalNotesRole` to `SessionModel::SessionRoles`.
   - Update `SessionData` struct to have a `QString localNotes;`.
   - In `SessionModel::parseSessionData`, populate `localNotes` from `obj.value("local_notes").toString()`.
   - In `SessionModel::data`, return `session.localNotes` for `LocalNotesRole`.
   - Add `void setLocalNotes(const QString &id, const QString &notes)` to `SessionModel` which updates the local note in the memory model, updates `rawObject["local_notes"]`, saves the sessions to disk using `saveSessions()`, and emits `dataChanged`.

2. **Add "Notes" tab to `SessionWindow`**
   - In `SessionWindow.h`, add `QTextEdit *m_notesEdit` and `QString m_sessionId;`.
   - In `SessionWindow.cpp::setupUi`, initialize `m_notesEdit`, add it to `m_tabWidget` as "Notes". Load its content from `sessionData.value("local_notes").toString()`.
   - To make it editable and save immediately: connect `m_notesEdit->textChanged` or a save button. Or use a `QPushButton` "Save Notes". But a cleaner way is just a standard text edit. However, we need to pass this data back to `MainWindow` so it saves to the cache. We can emit a new signal `notesChanged(id, text)` from `SessionWindow` when the text is modified, and use `QTimer::singleShot` for debouncing to not spam the disk. Let's just emit `notesChanged` when the text edit loses focus or `textChanged` (debounced). Better yet, just emit when the button "Save Notes" is clicked to make it simple. So, inside the Notes tab, put a QVBoxLayout with `m_notesEdit` and a `QPushButton` "Save Notes".
   - Or, emit `notesChanged` continuously.
   - Wait, `SessionWindow` is created with a copy of `QJsonObject sessionData`.

3. **Update "Details" view in `SessionWindow`**
   - In `SessionWindow::renderDetailsAndDiff()`, read `local_notes` from `m_sessionData`.
   - If not empty, append a snippet of it to the HTML. For example, the first 100 characters.
   - `detailsHtml += ... <h3>Notes</h3><p>...</p>`

4. **Add Context Menu Option in `MainWindow` & `SessionsWindow`**
   - The user asked to be able to right-click an item and choose "notes".
   - In `MainWindow`, for the various context menus (following sessions, archive sessions, queue, drafts), add an action "Notes".
   - When clicked, find the `id` of the selected row.
   - Call `m_apiManager->getSession(id)` or `openSessionWindow(id)`. Wait, we just open the `SessionWindow` and switch to the "Notes" tab.
   - To do this, we can add a method `showNotesTab()` to `SessionWindow` which does `m_tabWidget->setCurrentWidget(m_notesTabWidget);`
   - So in `MainWindow`, when right-clicking -> "Notes", we open the session window (either by instantiating it or reusing an existing one if possible), then call `window->showNotesTab()`. Since `MainWindow` just creates `new SessionWindow` currently, we'll create it and call `showNotesTab()`.

5. **Complete pre-commit steps**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.

6. **Submit**
   - Submit the branch.
