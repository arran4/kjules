1. **Add `hasUnreadChanges` to `SessionData`**: Update `src/sessionmodel.h` to include a `bool hasUnreadChanges = false;` in the `SessionData` struct.
2. **Add `UnreadChangesRole` to `SessionRoles`**: Add this new role in `SessionModel::SessionRoles`.
3. **Expose `Qt::FontRole` for Unread Changes**: In `SessionModel::data()`, if `session.hasUnreadChanges` is true, return a bolded `QFont`.
4. **Detect Changes on Update**: In `SessionModel::updateSession()`, compare the old and new state, PR status, and title. If any differ, set `hasUnreadChanges` to true. Make sure that changes are preserved when `hasUnreadChanges` is updated.
5. **Implement Clearing Methods**: Add `clearAllUnreadChanges()` and `clearUnreadChanges(QString)` to `SessionModel`.
6. **Clear on Refresh Following**: Call `clearAllUnreadChanges()` when the `m_refreshFollowingAction` is triggered in `MainWindow`.
7. **Clear on Session Open**: Call `clearUnreadChanges()` in `MainWindow::onSessionActivated()`.
8. **Build and Test**: Build the C++ project and run all relevant tests.
9. **Pre-commit Checks**: Complete pre commit steps to make sure proper testing, verifications, reviews and reflections are done.
10. **Submit Code**: Commit and push changes.
