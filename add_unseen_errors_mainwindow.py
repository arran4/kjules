import re

with open('src/mainwindow.h', 'r') as f:
    content = f.read()

content = re.sub(
    r'ClickableLabel \*m_statusLabel;',
    'ClickableLabel *m_statusLabel;\n  ClickableLabel *m_unseenErrorLabel;',
    content
)

content = re.sub(
    r'void onSourcesRefreshFinished\(\);',
    'void onSourcesRefreshFinished();\n  void onUnseenErrorsCountChanged(int count);',
    content
)

with open('src/mainwindow.h', 'w') as f:
    f.write(content)

with open('src/mainwindow.cpp', 'r') as f:
    content = f.read()

setup_sb_impl = '''  m_statusLabel = new ClickableLabel(i18n("Ready"), this);
  connect(m_statusLabel, &ClickableLabel::clicked, this, []() {
    ActivityLogWindow::instance()->show();
    ActivityLogWindow::instance()->raise();
    ActivityLogWindow::instance()->activateWindow();
  });
  statusBar()->addWidget(m_statusLabel);

  m_unseenErrorLabel = new ClickableLabel(this);
  m_unseenErrorLabel->hide(); // Initially hidden if no errors
  connect(m_unseenErrorLabel, &ClickableLabel::clicked, this, [this]() {
    ActivityLogWindow::instance()->show();
    ActivityLogWindow::instance()->raise();
    ActivityLogWindow::instance()->activateWindow();
    ActivityLogWindow::instance()->switchToErrorsTab();
  });
  statusBar()->addWidget(m_unseenErrorLabel);'''

content = re.sub(
    r'm_statusLabel = new ClickableLabel\(i18n\("Ready"\), this\);.*?statusBar\(\)->addWidget\(m_statusLabel\);',
    setup_sb_impl,
    content,
    flags=re.DOTALL
)

connect_unseen = '''  m_errorsModel = new ErrorsModel(this);
  connect(m_errorsModel, &ErrorsModel::unseenCountChanged, this, &MainWindow::onUnseenErrorsCountChanged);'''
content = re.sub(
    r'm_errorsModel = new ErrorsModel\(this\);',
    connect_unseen,
    content
)

new_method = '''
void MainWindow::onUnseenErrorsCountChanged(int count) {
  if (count > 0) {
    m_unseenErrorLabel->setText(i18np("[Error: %1]", "[Errors: %1]", count));
    m_unseenErrorLabel->show();
  } else {
    m_unseenErrorLabel->hide();
  }
}
'''
content += new_method

init_unseen = '''  updateSessionStats();
  onUnseenErrorsCountChanged(m_errorsModel->unseenCount());'''
content = re.sub(
    r'updateSessionStats\(\);',
    init_unseen,
    content
)

with open('src/mainwindow.cpp', 'w') as f:
    f.write(content)
