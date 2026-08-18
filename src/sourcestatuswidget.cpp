#include "sourcestatuswidget.h"
#include "blockedtreemodel.h"
#include "draftdelegate.h"
#include "errorsmodel.h"
#include "queuedelegate.h"
#include "queuemodel.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "sourcestatusdialog.h" // for proxy models

#include <QLabel>
#include <QListView>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

SourceStatusWidget::SourceStatusWidget(const QString &sourceName, SessionModel *sessionModel, QueueModel *queueModel,
                                       ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel, QWidget *parent)
    : QWidget(parent) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  QTabWidget *tabWidget = new QTabWidget(this);

  // In Progress
  QWidget *inProgressTab = new QWidget(this);
  QVBoxLayout *inProgressLayout = new QVBoxLayout(inProgressTab);
  QListView *inProgressView = new QListView(this);
  inProgressView->setItemDelegate(new SessionDelegate(inProgressView));
  SessionFilterProxyModel *sessionProxy = new SessionFilterProxyModel(sourceName, this);
  sessionProxy->setSourceModel(sessionModel);
  inProgressView->setModel(sessionProxy);
  inProgressLayout->addWidget(inProgressView);
  tabWidget->addTab(inProgressTab, tr("In Progress"));

  // Queue
  QWidget *queueTab = new QWidget(this);
  QVBoxLayout *queueLayout = new QVBoxLayout(queueTab);
  QListView *queueView = new QListView(this);
  queueView->setItemDelegate(new QueueDelegate(queueView));
  SourceFilterProxyModel *queueProxy = new SourceFilterProxyModel(sourceName, this);
  queueProxy->setSourceModel(queueModel);
  queueView->setModel(queueProxy);
  queueLayout->addWidget(queueView);
  tabWidget->addTab(queueTab, tr("In Queue"));

  // Blocked / Error
  QWidget *blockedTab = new QWidget(this);
  QVBoxLayout *blockedLayout = new QVBoxLayout(blockedTab);
  blockedLayout->addWidget(new QLabel(tr("Errors:"), this));
  QListView *errorView = new QListView(this);
  errorView->setItemDelegate(new DraftDelegate(errorView));
  ErrorFilterProxyModel *errorProxy = new ErrorFilterProxyModel(sourceName, this);
  errorProxy->setSourceModel(errorsModel);
  errorView->setModel(errorProxy);
  blockedLayout->addWidget(errorView);
  blockedLayout->addWidget(new QLabel(tr("Blocked Items:"), this));
  QTreeView *blockedView = new QTreeView(this);
  blockedView->setHeaderHidden(true);
  BlockedErrorProxyModel *blockedProxy = new BlockedErrorProxyModel(sourceName, this);
  blockedProxy->setSourceModel(blockedTreeModel);
  blockedView->setModel(blockedProxy);
  tabWidget->addTab(blockedTab, tr("Blocked / Error"));

  layout->addWidget(tabWidget);
}
