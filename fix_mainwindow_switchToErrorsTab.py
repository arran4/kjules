import re

with open('src/mainwindow.cpp', 'r') as f:
    content = f.read()

# Fix switchToErrorsTab error
switchToErrorTabStr = '''  m_unseenErrorLabel->hide(); // Initially hidden if no errors
  connect(m_unseenErrorLabel, &ClickableLabel::clicked, this, [this]() {
    ActivityLogWindow::instance()->show();
    ActivityLogWindow::instance()->raise();
    ActivityLogWindow::instance()->activateWindow();
    ActivityLogWindow::instance()->switchToErrorsTab();
  });'''

fixedSwitchToErrorTabStr = '''  m_unseenErrorLabel->hide(); // Initially hidden if no errors
  connect(m_unseenErrorLabel, &ClickableLabel::clicked, this, [this]() {
    for (int i = 0; i < m_tabWidget->count(); ++i) {
      if (m_tabWidget->widget(i)->objectName() == QStringLiteral("errorsTab")) {
        m_tabWidget->setCurrentIndex(i);
        break;
      }
    }
  });'''

content = content.replace(switchToErrorTabStr, fixedSwitchToErrorTabStr)

with open('src/mainwindow.cpp', 'w') as f:
    f.write(content)
