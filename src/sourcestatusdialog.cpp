#include "sourcestatusdialog.h"
#include "blockedtreemodel.h"
#include "draftdelegate.h" // used for errors
#include "errorsmodel.h"
#include "queuedelegate.h"
#include "queuemodel.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"

#include <QDialogButtonBox>
#include <QTreeView>

#include <QLabel>
#include <QListView>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

SourceFilterProxyModel::SourceFilterProxyModel(const QString &sourceName,
                                               QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

void SourceFilterProxyModel::setFilterSource(const QString &sourceName) {
  if (m_sourceName != sourceName) {
    m_sourceName = sourceName;
    invalidateFilter();
  }
}

bool SourceFilterProxyModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QJsonObject req =
      sourceModel()->data(index, QueueModel::RequestDataRole).toJsonObject();

  QString source = req.value(QStringLiteral("sourceContext"))
                       .toObject()
                       .value(QStringLiteral("source"))
                       .toString();
  if (source.isEmpty()) {
    source = req.value(QStringLiteral("source")).toString();
  }

  return source.endsWith(m_sourceName);
}

SessionFilterProxyModel::SessionFilterProxyModel(const QString &sourceName,
                                                 QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool SessionFilterProxyModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QString source =
      sourceModel()->data(index, SessionModel::SourceRole).toString();
  QString state =
      sourceModel()->data(index, SessionModel::StateRole).toString();

  return source == m_sourceName && (state == QStringLiteral("RUNNING") ||
                                    state == QStringLiteral("QUEUED"));
}

ErrorFilterProxyModel::ErrorFilterProxyModel(const QString &sourceName,
                                             QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool ErrorFilterProxyModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QJsonObject req =
      sourceModel()->data(index, ErrorsModel::RequestRole).toJsonObject();

  QString source = req.value(QStringLiteral("sourceContext"))
                       .toObject()
                       .value(QStringLiteral("source"))
                       .toString();
  if (source.isEmpty()) {
    source = req.value(QStringLiteral("source")).toString();
  }

  return source.endsWith(m_sourceName);
}

BlockedErrorProxyModel::BlockedErrorProxyModel(const QString &sourceName,
                                               QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool BlockedErrorProxyModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QString source =
      sourceModel()->data(index, BlockedTreeModel::SourceIdRole).toString();
  return source.endsWith(m_sourceName);
}

SourceStatusDialog::SourceStatusDialog(const QString &sourceName,
                                       SessionModel *sessionModel,
                                       QueueModel *queueModel,
                                       ErrorsModel *errorsModel,
                                       BlockedTreeModel *blockedTreeModel,
                                       QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Source Status: %1").arg(sourceName));
  resize(700, 500);

  QVBoxLayout *layout = new QVBoxLayout(this);

  QTabWidget *tabWidget = new QTabWidget(this);

  // In Progress
  QWidget *inProgressTab = new QWidget(this);
  QVBoxLayout *inProgressLayout = new QVBoxLayout(inProgressTab);
  QListView *inProgressView = new QListView(this);
  inProgressView->setItemDelegate(new SessionDelegate(inProgressView));
  SessionFilterProxyModel *sessionProxy =
      new SessionFilterProxyModel(sourceName, this);
  sessionProxy->setSourceModel(sessionModel);
  inProgressView->setModel(sessionProxy);
  inProgressLayout->addWidget(inProgressView);
  tabWidget->addTab(inProgressTab, tr("In Progress"));

  // Queue
  QWidget *queueTab = new QWidget(this);
  QVBoxLayout *queueLayout = new QVBoxLayout(queueTab);
  QListView *queueView = new QListView(this);
  queueView->setItemDelegate(new QueueDelegate(queueView));
  SourceFilterProxyModel *queueProxy =
      new SourceFilterProxyModel(sourceName, this);
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
  ErrorFilterProxyModel *errorProxy =
      new ErrorFilterProxyModel(sourceName, this);
  errorProxy->setSourceModel(errorsModel);
  errorView->setModel(errorProxy);
  blockedLayout->addWidget(errorView);
  blockedLayout->addWidget(new QLabel(tr("Blocked Items:"), this));
  QTreeView *blockedView = new QTreeView(this);
  blockedView->setHeaderHidden(true);
  BlockedErrorProxyModel *blockedProxy =
      new BlockedErrorProxyModel(sourceName, this);
  blockedProxy->setSourceModel(blockedTreeModel);
  blockedView->setModel(blockedProxy);
  tabWidget->addTab(blockedTab, tr("Blocked / Error"));

  layout->addWidget(tabWidget);

  QDialogButtonBox *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttonBox);
}
