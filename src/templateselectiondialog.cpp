#include "templateselectiondialog.h"
#include "templatesmodel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include "draftdelegate.h"

TemplateSelectionDialog::TemplateSelectionDialog(TemplatesModel *templatesModel, QWidget *parent)
    : QDialog(parent), m_templatesModel(templatesModel) {
  setWindowTitle(tr("Select Template"));
  resize(500, 400);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Search templates..."));
  mainLayout->addWidget(m_filterEdit);

  m_proxyModel = new QSortFilterProxyModel(this);
  m_proxyModel->setSourceModel(m_templatesModel);
  m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  // Filter on DisplayRole so empty names still match prompt
  m_proxyModel->setFilterRole(Qt::DisplayRole);

  m_listView = new QListView(this);
  m_listView->setModel(m_proxyModel);
  m_listView->setItemDelegate(new DraftDelegate(this));
  m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
  mainLayout->addWidget(m_listView);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  m_selectButton = new QPushButton(tr("Select"), this);
  m_selectButton->setEnabled(false);
  m_selectButton->setDefault(true);
  connect(m_selectButton, &QPushButton::clicked, this, &QDialog::accept);

  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(m_selectButton);

  mainLayout->addLayout(buttonLayout);

  connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    m_proxyModel->setFilterFixedString(text);
  });

  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &TemplateSelectionDialog::onSelectionChanged);
  connect(m_listView, &QListView::doubleClicked, this,
          &TemplateSelectionDialog::onDoubleClicked);
}

void TemplateSelectionDialog::onSelectionChanged() {
  QModelIndexList selected = m_listView->selectionModel()->selectedIndexes();
  if (!selected.isEmpty()) {
    QModelIndex proxyIndex = selected.first();
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    m_selectedTemplate = m_templatesModel->getTemplate(sourceIndex.row());
    m_selectButton->setEnabled(true);
  } else {
    m_selectButton->setEnabled(false);
  }
}

void TemplateSelectionDialog::onDoubleClicked() {
  onSelectionChanged();
  if (m_selectButton->isEnabled()) {
    accept();
  }
}

QJsonObject TemplateSelectionDialog::selectedTemplate() const {
  return m_selectedTemplate;
}
