with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

# Fix 11. Remove duplicate Activity Log reporting
# MainWindow::onError() currently calls updateStatus(), which already logs to ActivityLogWindow, then explicitly logs the same message again.
if "m_activityLogWindow->logError(message);" in content:
    content = content.replace("m_activityLogWindow->logError(message);", "// m_activityLogWindow->logError(message); // updateStatus already logs it.")

# Fix 10. Finish MainWindow unseen-count wiring
if "connect(m_errorsModel," not in content or "unseenCountChanged" not in content:
    idx = content.find("onUnseenErrorsCountChanged(m_errorsModel->unseenCount());")
    if idx != -1:
        insert_str = """
  connect(m_errorsModel,
          &ErrorsModel::unseenCountChanged,
          this,
          &MainWindow::onUnseenErrorsCountChanged);
"""
        content = content[:idx] + insert_str + content[idx:]

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
