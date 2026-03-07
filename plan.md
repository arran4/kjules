1. **Update `SessionModel` (`src/sessionmodel.h`, `src/sessionmodel.cpp`)**:
   - Add new columns: `ColUpdatedAt`, `ColCreatedAt`, `ColProvider`, `ColOwner`, `ColRepo`. Update `ColCount`.
   - Update `SessionData` to hold: `QDateTime updateTime`, `QDateTime createTime`, `QString provider`, `QString owner`, `QString repo`.
   - Create a helper `parseSessionData(const QJsonObject& obj)` to populate `SessionData` to keep it DRY.
   - Truncate title and remove newlines. Fallback to `prompt` if title is empty.
   - Use parsing logic to split the source into provider, owner, repo (e.g., `sources/github/owner/repo`).

2. **Update `SessionsWindow` (`src/sessionswindow.cpp`)**:
   - Configure default sort (Updated At, Descending).
   - Set minimum sizes for columns: `m_listView->header()->setMinimumSectionSize(...)` or `resizeSection(...)`.
   - Handle context menu right clicks:
     - Get `provider`, `owner`, `repo` from model or source URL.
     - Generate openable/copyable URLs and add them to context menu (similar to `MainWindow`'s handling).
   - Show `SessionWindow` details popup on double-click instead of just fetching. Actually, fetching triggers `sessionDetailsReceived` in `MainWindow`, which shows `SessionWindow`. I'll confirm this, or instantiate it directly if data is enough, but API fetch gets full data.
     Wait, the user says "Create a simple popup window for each one to show more details that we will expand on in a bit." I'll create a `SessionDetailDialog` or just use a simple `QDialog` with a `QTextBrowser` for now.

3. **Preferences / Auto-load settings**:
   - Create a configuration file or section (`SessionsWindow` settings) using `KSharedConfig`.
   - Options: "Auto Load Behavior" with choices: "Manual", "Load All On Refresh", "Auto-load when scrolled to bottom" (mock the option for now).
   - Add a settings dialog or just a simple menu `Options -> Auto Load Behavior -> ...` in `SessionsWindow`'s menu bar.

4. **Complex Filtering**:
   - Add a filter widget at the top with a text search (`QLineEdit`) and status dropdown (`QComboBox`).
   - Implement `SessionsProxyModel` subclass of `QSortFilterProxyModel` that filters by title/prompt, status, and source.

5. **Execute tests and review**.
