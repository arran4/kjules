#include "templatesmodel.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

TemplatesModel::TemplatesModel(QObject *parent) : QAbstractListModel(parent) {
  loadTemplates();
}

int TemplatesModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_templates.size();
}

QVariant TemplatesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_templates.size())
    return QVariant();

  QJsonObject tmpl = m_templates[index.row()].toObject();

  if (role == Qt::DisplayRole || role == PromptRole) {
    QString prompt = tmpl.value(QStringLiteral("prompt")).toString();
    if (role == Qt::DisplayRole) {
      QString display = prompt;
      display.replace(QLatin1Char('\n'), QLatin1Char(' '));
      if (display.length() > 50) {
        display = display.left(47) + QStringLiteral("...");
      }
      return display.isEmpty() ? QStringLiteral("Empty Template") : display;
    }
    return prompt;
  } else if (role == AutomationModeRole) {
    return tmpl.value(QStringLiteral("automationMode")).toString();
  }

  return QVariant();
}

QHash<int, QByteArray> TemplatesModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[PromptRole] = "prompt";
  roles[AutomationModeRole] = "automationMode";
  return roles;
}

void TemplatesModel::addTemplate(const QJsonObject &templateData) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_templates.insert(0, templateData);
  endInsertRows();
  saveTemplates();
}

void TemplatesModel::removeTemplate(int row) {
  if (row < 0 || row >= m_templates.size())
    return;

  beginRemoveRows(QModelIndex(), row, row);
  m_templates.removeAt(row);
  endRemoveRows();
  saveTemplates();
}

QJsonObject TemplatesModel::getTemplate(int row) const {
  if (row >= 0 && row < m_templates.size()) {
    return m_templates[row].toObject();
  }
  return QJsonObject();
}

void TemplatesModel::loadTemplates() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/templates.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    beginResetModel();
    m_templates = doc.array();
    endResetModel();
    file.close();
  }
}

void TemplatesModel::saveTemplates() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(path + QStringLiteral("/templates.json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonDocument doc(m_templates);
    file.write(doc.toJson());
    file.close();
  }
}

void TemplatesModel::clear() {
  beginResetModel();
  m_templates = QJsonArray();
  endResetModel();
  saveTemplates();
}
