1. **Modify `MainWindow::onQueueContextMenu` in `src/mainwindow.cpp`**
   - Identify if the selected item is a wait item (`item.isWaitItem`).
   - If it is a wait item, create a submenu `Modify Time`.
   - Add options to reduce remaining time by 10%, 50%, 90% and increase remaining time by 10%, 50%, 90%.
   - Store these actions in a map to their multiplier values.
   - For wait items, consider conditionally hiding some actions that don't make sense (like "Edit", "Convert to Draft", "Copy as Template", "Send Now") or keep them if they might have a use, but generally they are useless for wait items.
   - After `menu.exec()`, if a modify action was selected, calculate the new wait time and update the item in the model.
2. **Pre-commit checks**
   - Run `pre_commit_instructions` tool to verify checks.
3. **Submit**
   - Create a commit and submit.
