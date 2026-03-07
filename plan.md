1. **API Manager changes**
   - Add `m_listSessionsReply` in `APIManager` constructor and destructor.
   - Update `listSessions(const QString &pageToken = QString())` in `apimanager.h` and `.cpp` to append `?pageToken=...`, auto-paginate if `nextPageToken` is found, and emit `sessionsRefreshFinished()` on completion or cancel. Emit `sessionsReceived` for each chunk of data.
   - Add `cancelListSessions()` in `apimanager.h` and `.cpp`.

2. **Session Model changes**
   - In `sessionmodel.h`, add `int addSessions(const QJsonArray &sessions);`, `void loadSessions();`, and `void saveSessions();` to manage cache in `cached_sessions_list.json` instead of just an in-memory list or the local hardcoded cached one in MainWindow. Actually wait, `MainWindow` loads/saves past sessions to `cached_sessions.json`. Let's let `SessionsWindow` manage its own cache or `SessionModel` manage it if possible. The prompt asks for: "It caches, it has refresh, pagination, etc." Let's create a dedicated `SessionModel` instance for the `SessionsWindow`. The `SessionsWindow` will load/save from `cached_all_sessions.json`.
   - Update `sessionmodel.cpp` to implement `addSessions` (returning the number of items added).

3. **Create Sessions Window**
   - Create `src/sessionswindow.h` and `src/sessionswindow.cpp`.
   - It should inherit `KXmlGuiWindow`.
   - Setup a `QTreeView` or `QListView` (we'll use `QListView` like `Past` tab, or `QTreeView` for filtering if needed, but `QListView` is standard for sessions in this app) showing all sessions from jules API. The prompt says "It should have a toolbar, menu, and status bar and work like sources except for sessions basically. There is only one tab. 'All sessions'."
   - We will use `QListView` and a `QSortFilterProxyModel` connected to `m_sessionModel`.
   - Implement caching using `m_sessionModel->setSessions` / `QJsonDocument` loading from `cached_all_sessions.json`.
   - Implement pagination with a status bar (like `sources` tab).
   - "You can open a filtered version of it which isn't cached, by right clicking on a session and going 'view sessions'...". Actually, in `mainwindow.cpp` there is a `m_viewSessionsAction` that opens a filtered view of sessions for a *source*. We should update `m_viewSessionsAction` to use `SessionsWindow` in a filtered, non-cached mode.

4. **Integration**
   - In `mainwindow.cpp`, when "View Sessions" is clicked (from toolbar, menu, or tray), open the `SessionsWindow` in default mode (cached, loads full list).
   - In `mainwindow.cpp`, `m_viewSessionsAction` is currently for a source context menu. Update it to open `SessionsWindow` but filter it for that specific source. Note that "filtered version of it which isn't cached" implies we should pass a parameter to `SessionsWindow` to not save cache and maybe pre-filter.
   - Wait, `mainwindow.cpp` already has `m_showFullSessionListAction` and `m_viewSessionsAction`. We can replace the ad-hoc windows created there with `SessionsWindow`.

5. **Files to add to CMakeLists.txt**
   - `src/sessionswindow.cpp`

6. **Pre-commit checks**
   - Follow instructions.
