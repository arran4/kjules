#include "templateeditdialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

TemplateEditDialog::TemplateEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Edit Template"));
  resize(500, 400);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QFormLayout *formLayout = new QFormLayout();

  m_nameEdit = new QLineEdit(this);
  formLayout->addRow(tr("Name:"), m_nameEdit);

  m_descEdit = new QTextEdit(this);
  m_descEdit->setMaximumHeight(80);
  formLayout->addRow(tr("Description:"), m_descEdit);

  m_promptEdit = new QTextEdit(this);
  formLayout->addRow(tr("Prompt:"), m_promptEdit);

  m_requirePlanApprovalCheckBox = new QCheckBox(this);
  formLayout->addRow(tr("Require Plan Approval:"), m_requirePlanApprovalCheckBox);

  mainLayout->addLayout(formLayout);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  QPushButton *saveBtn = new QPushButton(tr("Save"), this);
  saveBtn->setDefault(true);
  connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);

  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelBtn);
  buttonLayout->addWidget(saveBtn);

  mainLayout->addLayout(buttonLayout);
}

void TemplateEditDialog::setInitialData(const QJsonObject &data) {
  m_originalData = data;
  m_nameEdit->setText(data.value(QStringLiteral("name")).toString());
  m_descEdit->setPlainText(data.value(QStringLiteral("description")).toString());
  m_promptEdit->setPlainText(data.value(QStringLiteral("prompt")).toString());

  if (data.contains(QStringLiteral("requirePlanApproval"))) {
    m_requirePlanApprovalCheckBox->setChecked(data.value(QStringLiteral("requirePlanApproval")).toBool());
  } else {
    m_requirePlanApprovalCheckBox->setChecked(false);
  }
}

QJsonObject TemplateEditDialog::templateData() const {
  QJsonObject data = m_originalData;
  data[QStringLiteral("name")] = m_nameEdit->text();
  data[QStringLiteral("description")] = m_descEdit->toPlainText();
  data[QStringLiteral("prompt")] = m_promptEdit->toPlainText();
  data[QStringLiteral("requirePlanApproval")] = m_requirePlanApprovalCheckBox->isChecked();
  return data;
}
