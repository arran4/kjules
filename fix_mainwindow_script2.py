import re

with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

content = content.replace(
    'SessionModel *localModel = new SessionModel(sessionsWindow);',
    'SessionModel *localModel = new SessionModel(QStringLiteral("cached_all_sessions.json"), sessionsWindow);'
)

backup_old = """      if (filesToBackup.contains(QStringLiteral("cached_sessions.json")) ||
          filesToBackup.contains(QStringLiteral("cached_all_sessions.json")))
        m_sessionModel->clear();"""

backup_new = """      if (filesToBackup.contains(QStringLiteral("cached_sessions.json")) ||
          filesToBackup.contains(QStringLiteral("cached_all_sessions.json")))
        m_sessionModel->clear();
      if (filesToBackup.contains(QStringLiteral("cached_archive_sessions.json")))
        m_archiveModel->clear();"""

content = content.replace(backup_old, backup_new)

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
