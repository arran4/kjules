#include "savedialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

SaveDialog::SaveDialog(const QString &type, QWidget *parent)
    : QDialog(parent), m_type(type) {
  setWindowTitle(QStringLiteral("Save as ") + type);
  resize(400, type == QStringLiteral("Template") ? 300 : 150);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QFormLayout *formLayout = new QFormLayout();

  m_nameEdit = new QLineEdit(this);
  m_descEdit = new QTextEdit(this);

  if (type == QStringLiteral("Template")) {
    formLayout->addRow(tr("Name:"), m_nameEdit);
    formLayout->addRow(tr("Description:"), m_descEdit);
  } else if (type == QStringLiteral("Draft")) {
    formLayout->addRow(tr("Comment:"), m_nameEdit);
    // Drafts do not need a description based on requirements
    m_descEdit->hide();
  }

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

void SaveDialog::setInitialData(const QString &nameOrComment,
                                const QString &description) {
  m_nameEdit->setText(nameOrComment);
  m_descEdit->setPlainText(description);
}

QString SaveDialog::nameOrComment() const { return m_nameEdit->text(); }

QString SaveDialog::description() const { return m_descEdit->toPlainText(); }
