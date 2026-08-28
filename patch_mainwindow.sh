sed -i '/void showNewSessionDialog/i \public Q_SLOTS:\n  void showNewSessionDialogSlot() { showNewSessionDialog(); }\nprivate Q_SLOTS:' src/mainwindow.h
