#ifndef SOURCESTATUSDIALOG_H
#define SOURCESTATUSDIALOG_H

#include <QDialog>
class SessionModel;
class QueueModel;
class ErrorsModel;
class BlockedTreeModel;

class SourceStatusDialog : public QDialog {
  Q_OBJECT
public:
  explicit SourceStatusDialog(const QString &sourceName, SessionModel *sessionModel, QueueModel *queueModel,
                              ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel, QWidget *parent = nullptr);
};

#endif // SOURCESTATUSDIALOG_H
