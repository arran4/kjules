1. **Spacing for "Name" column**:
   - In `MainWindow::setupUi()`, for `m_sourceView`, set `header()->setSectionResizeMode(SourceModel::ColName, QHeaderView::Stretch);` or `header()->resizeSection(SourceModel::ColName, 300);`. The user asked to "provide a lot more spacing to the name."

2. **isPrivate icon or column**:
   - In `SourceModel::data` for `Qt::DecorationRole` and `index.column() == ColName`, check if `source.value("isPrivate").toBool()` is true. If so, return a lock icon `QIcon::fromTheme("security-high")` or similar.

3. **Replace Created/Updated with First Seen / Last Changed**:
   - Modify `SourceModel::Columns` to replace `ColCreated` with `ColFirstSeen`, and `ColUpdated` with `ColLastChanged`.
   - Update `data()` and `headerData()` accordingly.
   - For `firstSeen`: in `addSources` or `setSources`, if it's a completely new source (not existing), set `source["local_firstSeen"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate)`. Also preserve it on updates.
   - For `lastChanged`: in `updateSource`, if the JSON object actually changed compared to the local version, or just set it whenever `addSources` or `updateSource` or `recordSessionCreated` changes the source locally? The user said "Last changed" or something. Let's just update `local_lastChanged` whenever `recordSessionCreated` happens, or maybe when the source is first seen/updated from the backend. "Last changed" usually means last changed by us, or last time we saw a change from the API? The easiest is to just track `local_firstSeen` and `local_lastChanged`.
   - Wait, `addSources` and `setSources` receive API data. Let's set `local_firstSeen` when a new source is added. Set `local_lastChanged` when a source is updated.

4. **"Last used should update everytime I create a new session."**
   - It *does* update, but maybe they want `SourceModel::data()` to format `local_lastUsed` more visibly, or they mean it wasn't updating before my previous fix. I'll make sure it's preserved properly.

Let's write a script to do these.
