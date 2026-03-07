#include "newsessiondialog.h"
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QVBoxLayout>

NewSessionDialog::NewSessionDialog(SourceModel *sourceModel, bool hasApiKey,
                                   QWidget *parent)
    : QDialog(parent), m_sourceModel(sourceModel) {
  setWindowTitle(tr("Create New Session"));
  resize(700, 600);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFormLayout *formLayout = new QFormLayout();

  // Source Selection
  QVBoxLayout *sourceLayout = new QVBoxLayout();

  QHBoxLayout *filterLayout = new QHBoxLayout();
  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter sources..."));
  filterLayout->addWidget(m_filterEdit);

  QPushButton *selectAllBtn = new QPushButton(tr("Select All"), this);
  connect(selectAllBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onSelectAll);

  QPushButton *unselectAllBtn = new QPushButton(tr("Unselect All"), this);
  connect(unselectAllBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onUnselectAll);

  filterLayout->addWidget(selectAllBtn);
  filterLayout->addWidget(unselectAllBtn);

  sourceLayout->addLayout(filterLayout);

  m_sourceView = new QListView(this);

  QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
  proxyModel->setSourceModel(m_sourceModel);
  proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxyModel->setFilterRole(SourceModel::NameRole);

  connect(m_filterEdit, &QLineEdit::textChanged, proxyModel,
          &QSortFilterProxyModel::setFilterFixedString);

  m_sourceView->setModel(proxyModel);
  m_sourceView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_sourceView->setFixedHeight(200);
  sourceLayout->addWidget(m_sourceView);

  formLayout->addRow(tr("Sources:"), sourceLayout);

  // Prompt
  m_promptEdit = new QTextEdit(this);
  formLayout->addRow(tr("Prompt:"), m_promptEdit);

  mainLayout->addLayout(formLayout);

  // Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  QPushButton *draftButton = new QPushButton(tr("Save as Draft"), this);
  connect(draftButton, &QPushButton::clicked, this,
          &NewSessionDialog::onSaveDraft);

  m_createButton = new QPushButton(tr("Create Session"), this);
  if (!hasApiKey) {
    m_createButton->setEnabled(false);
    m_createButton->setToolTip(
        tr("An API key is required to create a session."));
  }
  connect(m_createButton, &QPushButton::clicked, this,
          [this]() { onSubmit(QStringLiteral("")); });

  m_createPRButton = new QPushButton(tr("Create PR Session"), this);
  m_createPRButton->setDefault(true);
  if (!hasApiKey) {
    m_createPRButton->setEnabled(false);
    m_createPRButton->setToolTip(
        tr("An API key is required to create a session."));
  }
  connect(m_createPRButton, &QPushButton::clicked, this,
          [this]() { onSubmit(QStringLiteral("AUTO_CREATE_PR")); });

  buttonLayout->addWidget(cancelButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(draftButton);
  buttonLayout->addWidget(m_createButton);
  buttonLayout->addWidget(m_createPRButton);

  mainLayout->addLayout(buttonLayout);
}

void NewSessionDialog::setEditMode(bool isEdit) {
  if (isEdit) {
    setWindowTitle(tr("Edit Queued Session"));
    m_createButton->setText(tr("Requeue Session"));
    m_createPRButton->setText(tr("Requeue PR Session"));
  } else {
    setWindowTitle(tr("Create New Session"));
    m_createButton->setText(tr("Create Session"));
    m_createPRButton->setText(tr("Create PR Session"));
  }
}

void NewSessionDialog::setInitialData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();

  // Check for "sources" array, fallback to "source" string
  QStringList sources;
  if (data.contains(QStringLiteral("sources"))) {
    QJsonArray arr = data.value(QStringLiteral("sources")).toArray();
    for (const auto &val : arr) {
      sources.append(val.toString());
    }
  } else if (data.contains(QStringLiteral("source"))) {
    sources.append(data.value(QStringLiteral("source")).toString());
  }

  m_promptEdit->setPlainText(prompt);

  // Select sources
  QAbstractItemModel *model = m_sourceView->model(); // Proxy model
  QItemSelectionModel *selectionModel = m_sourceView->selectionModel();
  selectionModel->clearSelection();

  QSet<QString> sourcesSet(sources.begin(), sources.end());

  for (int i = 0; i < model->rowCount(); ++i) {
    QModelIndex idx = model->index(i, 0);
    QString name = model->data(idx, SourceModel::NameRole).toString();

    if (sourcesSet.contains(name)) {
      selectionModel->select(idx, QItemSelectionModel::Select);
    }
  }
}

void NewSessionDialog::onSubmit(const QString &automationMode) {
  QModelIndexList selection = m_sourceView->selectionModel()->selectedIndexes();
  if (selection.isEmpty()) {
    QMessageBox::warning(this, tr("Missing Source"),
                         tr("Please select at least one source."));
    return;
  }

  QStringList sources;
  for (const QModelIndex &idx : selection) {
    sources.append(idx.data(SourceModel::NameRole).toString());
  }

  QString prompt = m_promptEdit->toPlainText();

  if (prompt.isEmpty()) {
    QMessageBox::warning(this, tr("Missing Prompt"),
                         tr("Please enter a prompt."));
    return;
  }

  Q_EMIT createSessionRequested(sources, prompt, automationMode);
  accept();
}

void NewSessionDialog::onSaveDraft() {
  QModelIndexList selection = m_sourceView->selectionModel()->selectedIndexes();
  QJsonArray sourcesArr;
  for (const QModelIndex &idx : selection) {
    sourcesArr.append(idx.data(SourceModel::NameRole).toString());
  }

  QString prompt = m_promptEdit->toPlainText();

  QJsonObject draft;
  draft[QStringLiteral("sources")] = sourcesArr;
  draft[QStringLiteral("prompt")] = prompt;

  Q_EMIT saveDraftRequested(draft);
  accept();
}

void NewSessionDialog::onSelectAll() { m_sourceView->selectAll(); }

void NewSessionDialog::onUnselectAll() { m_sourceView->clearSelection(); }
