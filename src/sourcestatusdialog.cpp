#include "sourcestatusdialog.h"
#include "sourcestatuswidget.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

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
