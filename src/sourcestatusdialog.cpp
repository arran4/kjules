#include "sourcestatusdialog.h"
#include "blockedtreemodel.h"
#include "errorsmodel.h"
#include "queuemodel.h"
#include "sessionmodel.h"
#include "sourcestatuswidget.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

SourceFilterProxyModel::SourceFilterProxyModel(const QString &sourceName, QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

void SourceFilterProxyModel::setFilterSource(const QString &sourceName) {
  if (m_sourceName != sourceName) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_sourceName = sourceName;
    endFilterChange();
#else
    m_sourceName = sourceName;
    invalidateFilter();
#endif
  }
}

bool SourceFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QJsonObject req = sourceModel()->data(index, QueueModel::RequestDataRole).toJsonObject();

  QString source = req.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
  if (source.isEmpty()) {
    source = req.value(QStringLiteral("source")).toString();
  }

  return source == m_sourceName;
}

SessionFilterProxyModel::SessionFilterProxyModel(const QString &sourceName, QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool SessionFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QString source = sourceModel()->data(index, SessionModel::SourceRole).toString();
  QString state = sourceModel()->data(index, SessionModel::StateRole).toString();

  return source == m_sourceName && (state == QStringLiteral("RUNNING") || state == QStringLiteral("QUEUED"));
}

ErrorFilterProxyModel::ErrorFilterProxyModel(const QString &sourceName, QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool ErrorFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QJsonObject req = sourceModel()->data(index, ErrorsModel::RequestRole).toJsonObject();

  QString source = req.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
  if (source.isEmpty()) {
    source = req.value(QStringLiteral("source")).toString();
  }

  return source == m_sourceName;
}

BlockedErrorProxyModel::BlockedErrorProxyModel(const QString &sourceName, QObject *parent)
    : QSortFilterProxyModel(parent), m_sourceName(sourceName) {}

bool BlockedErrorProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  QString source = sourceModel()->data(index, BlockedTreeModel::SourceIdRole).toString();
  return source == m_sourceName;
}

SourceStatusDialog::SourceStatusDialog(const QString &sourceName, SessionModel *sessionModel, QueueModel *queueModel,
                                       ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Source Status: %1").arg(sourceName));
  resize(700, 500);

  QVBoxLayout *layout = new QVBoxLayout(this);

  SourceStatusWidget *statusWidget =
      new SourceStatusWidget(sourceName, sessionModel, queueModel, errorsModel, blockedTreeModel, this);
  layout->addWidget(statusWidget);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttonBox);
}
