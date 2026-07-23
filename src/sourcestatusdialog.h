#ifndef SOURCESTATUSDIALOG_H
#define SOURCESTATUSDIALOG_H

#include <QDialog>
#include <QSortFilterProxyModel>

class SessionModel;
class QueueModel;
class ErrorsModel;
class BlockedTreeModel;

class SourceFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SourceFilterProxyModel(const QString &sourceName, QObject *parent = nullptr);
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  void setFilterSource(const QString &sourceName);

private:
  QString m_sourceName;
};

class SessionFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionFilterProxyModel(const QString &sourceName, QObject *parent = nullptr);
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  QString m_sourceName;
};

class ErrorFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit ErrorFilterProxyModel(const QString &sourceName, QObject *parent = nullptr);
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  QString m_sourceName;
};
class BlockedErrorProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit BlockedErrorProxyModel(const QString &sourceName, QObject *parent = nullptr);
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  QString m_sourceName;
};

class SourceStatusDialog : public QDialog {
  Q_OBJECT
public:
  explicit SourceStatusDialog(const QString &sourceName, SessionModel *sessionModel, QueueModel *queueModel,
                              ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel, QWidget *parent = nullptr);
};

#endif // SOURCESTATUSDIALOG_H
