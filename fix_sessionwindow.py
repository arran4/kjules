import re

with open("src/sessionwindow.cpp", "r") as f:
    content = f.read()

# Fix the manual widget insertion block removal which I messed up because of indentation differences
block_to_remove = """  if (auto *tb = toolBar(QStringLiteral("mainToolBar"))) {
    QAction *closeAct =
        actionCollection()->action(QStringLiteral("close_window"));
    tb->insertWidget(closeAct, new QLabel(i18n(" Auto Refresh: "), this));
    tb->insertWidget(closeAct, m_autoRefreshCombo);
    tb->insertSeparator(closeAct);
    tb->show();
  }
"""
content = content.replace(block_to_remove, "")

# also add include for QWidgetAction and QHBoxLayout
if "#include <QWidgetAction>" not in content:
    content = content.replace("#include <QVBoxLayout>", "#include <QVBoxLayout>\n#include <QWidgetAction>\n#include <QHBoxLayout>")

with open("src/sessionwindow.cpp", "w") as f:
    f.write(content)
