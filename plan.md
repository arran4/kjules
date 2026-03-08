1. **Multi-select & Actions**:
   - In `SessionsWindow`, set the `QTreeView` selection mode to `ExtendedSelection` to allow multi-select.
   - Update the context menu actions ("Open Session URL", "Copy Session URL", "Open Source URL", etc.) to iterate over all selected rows and execute the action (e.g. open all URLs, or copy multiple URLs joined by newlines).
2. **Reload specific rows**:
   - Add a new "Reload Selected" action to the context menu. For each selected row, fetch the specific session detail via `m_apiManager->getSession(id)`.
   - Need to wire up the `sessionDetailsReceived` signal in `SessionsWindow` to update the specific session in the model using `m_model->updateSession(session)`.
3. **Load Remaining Pages**:
   - Add a "Load Remaining" option (perhaps to the menu or toolbar) that acts similarly to `resumeRefresh()` but ignores the auto-load preference and forcefully loops `resumeRefresh()` until `m_nextPageToken` is empty. Can be a boolean flag `m_isLoadingRemaining` inside `SessionsWindow`.
4. **Filter by Repo**:
   - Add a `QComboBox` for filtering by repository. Populate this dropdown dynamically based on the unique repos currently loaded in the model.
   - Add a `setRepoFilter` to `SessionsProxyModel`. Update `filterAcceptsRow` to respect this new filter.
   - We might need to listen to `modelReset` or `rowsInserted` on `m_model` to update the Repo dropdown items in `SessionsWindow`.
