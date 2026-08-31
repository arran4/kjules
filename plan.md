1. **Restore all state and create legacy data repair helper header**
   - Use `write_file` to create `src/legacydatarepair.h`.
   - Define `struct LegacyDataRepairResult` with `legacyFollowingCount`, `currentFollowingCount`, `followingToRecover`, `followingAlreadyPresent`, `legacyQueueCount`, `currentQueueCount`, `queueToRecover`, `queueAlreadyPresent`, `error` string.
   - Define `class LegacyDataRepair` with `analyze(SessionModel*, QueueModel*)`, `performMerge(SessionModel*, QueueModel*)`, and private helper methods for path resolution and parsing logic, including `outIsMalformed` boolean reference for parsing functions.
   - Verify creation with `read_file`.

2. **Implement legacy data repair helper source logic**
   - Use `write_file` to create a basic skeleton for `src/legacydatarepair.cpp`.
   - Use `replace_with_git_merge_diff` to add path location logic (using `QStandardPaths::GenericDataLocation` + "/org.kde.kjules" and `QStandardPaths::AppDataLocation`).
   - Use `replace_with_git_merge_diff` to add parsing logic for `queue.json` and `cached_all_sessions.json` that handles both bare-array and object forms, setting `outIsMalformed` if a file is present but contains invalid JSON.
   - Use `replace_with_git_merge_diff` to implement the multiplicity-aware matching logic for Queue items.
   - Use `replace_with_git_merge_diff` to implement ID-based matching for Following sessions.
   - Use `replace_with_git_merge_diff` to implement backup logic (creating timestamped directory and copying current files).
   - Use `replace_with_git_merge_diff` to implement `performMerge()` making sure to invoke `SessionModel::addSessions()` with filtered items and using `QueueModel::beginBatchUpdate()`, `QueueModel::insertItem(queueModel->size(), ...)` (to strictly append items at the end, without priority insertion), and `QueueModel::endBatchUpdate()`.
   - Verify contents using `read_file`.

3. **Update CMakeLists.txt**
   - Use `replace_with_git_merge_diff` to append `src/legacydatarepair.cpp` to the file list containing `src/mainwindow.cpp` in `CMakeLists.txt`.
   - Use `replace_with_git_merge_diff` to add `add_executable(test_legacydatarepair ...)` to `tests/CMakeLists.txt` and link to `Qt6::Test Qt6::Widgets Qt6::Core KF6::I18n KF6::ConfigCore KF6::ConfigWidgets KF6::CoreAddons`.
   - Verify changes with `read_file`

4. **Update UI / `MainWindow`**
   - Modify `src/kjulesui.rc` with `replace_with_git_merge_diff` to nest `<Action name="fix_sources" />` and `<Action name="merge_legacy_queue_following" />` inside a `<Menu name="one_time_fixes">` submenu under `<Menu name="help">`.
   - Modify `src/mainwindow.h` using `replace_with_git_merge_diff` to add `QAction *m_mergeLegacyDataAction;` and `void mergeLegacyData();`.
   - Modify `src/mainwindow.cpp` using `replace_with_git_merge_diff` to construct the new action, connect it to `mergeLegacyData()`, and add it to the menu.
   - Implement `MainWindow::mergeLegacyData()` with `replace_with_git_merge_diff`. It will use `m_sessionModel` and `m_queueModel`, display a confirmation dialog showing the calculated counts, call `performMerge`, then show an error if one occurred, otherwise report the success result in the status bar.
   - Verify changes using `read_file`.

5. **Write tests**
   - Use `write_file` to create `tests/test_legacydatarepair.cpp`.
   - Add tests for Legacy Following only and overlap logic (current duplicates win).
   - Add tests for empty session IDs skipped safely and bare-array format.
   - Add tests for Legacy Queue only and Queue overlap.
   - Add tests for Multiplicity (current 1, legacy 2 -> recover 1).
   - Add tests ensuring legacy `m_runTimestamps` are ignored, metadata fields are retained, and bare-array Queue formats parse properly.
   - Add tests for Invalid JSON, Missing legacy files, Backup failure mock, and Idempotence (running merge twice adds nothing).
   - Verify test file was written using `read_file`.

6. **Run Tests**
   - Compile and execute tests with `.jules/run.sh cmake --build build --parallel && .jules/run.sh ctest --test-dir build --output-on-failure`.

7. **Pre-commit Instructions**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
