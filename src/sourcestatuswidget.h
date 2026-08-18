#ifndef SOURCESTATUSWIDGET_H
#define SOURCESTATUSWIDGET_H

#include <QWidget>

class SessionModel;
class QueueModel;
class ErrorsModel;
class BlockedTreeModel;

class SourceStatusWidget : public QWidget {
  Q_OBJECT
public:
  explicit SourceStatusWidget(const QString &sourceName, SessionModel *sessionModel, QueueModel *queueModel,
                              ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel, QWidget *parent = nullptr);
};

#endif // SOURCESTATUSWIDGET_H
