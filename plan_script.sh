#!/bin/bash
cat << 'REPLACE_EOF' > /tmp/replace.diff
<<<<<<< SEARCH
  if (restoredSomething) {
    updateStatus(i18n("Data restored successfully. Please restart the application or refresh models."));
  } else {
    updateStatus(i18n("No files were restored."));
  }
}
=======
  if (restoredSomething) {
    updateStatus(i18n("Data restored successfully."));

    // Clear models so they reload properly
    if (filesToRestore.contains(QStringLiteral("sources.json"))) {
      m_sourceModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("cached_sessions.json")) ||
        filesToRestore.contains(QStringLiteral("cached_all_sessions.json"))) {
      m_sessionModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("drafts.json"))) {
      m_draftsModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("queue.json"))) {
      m_queueModel->clear();
    }
    if (filesToRestore.contains(QStringLiteral("errors.json"))) {
      m_errorsModel->clear();
    }

    // Trigger a refresh to pull the newly restored data from the backend/disk
    refreshSources();

    // Attempt to load settings again if queue settings changed
    loadQueueSettings();

  } else {
    updateStatus(i18n("No files were restored."));
  }
}
>>>>>>> REPLACE
REPLACE_EOF
patch -p1 src/mainwindow.cpp < /tmp/replace.diff || true
