#include "templatesmodel.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
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

  const QJsonObject tmpl = m_templates[index.row()].toObject();

  switch (role) {
  case SourceRole: {
    if (tmpl.contains(QStringLiteral("sources"))) {
      QJsonArray sourcesArray =
          tmpl.value(QStringLiteral("sources")).toArray();
      QStringList sourcesList;
      for (const QJsonValue &val : sourcesArray) {
        sourcesList.append(val.toString());
      }
      return sourcesList.join(QStringLiteral(", "));
    }
    return tmpl.value(QStringLiteral("source")).toString();
  }
  case PromptRole:
    return tmpl.value(QStringLiteral("prompt")).toString();
  case AutomationModeRole:
    return tmpl.value(QStringLiteral("automationMode")).toString();
  case TemplatesModel::NameRole:
    return tmpl.value(QStringLiteral("name")).toString();
  case TemplatesModel::DescriptionRole:
    return tmpl.value(QStringLiteral("description")).toString();
  case Qt::DisplayRole: {
    QString name = tmpl.value(QStringLiteral("name")).toString();
    if (!name.isEmpty()) return name;
    return tmpl.value(QStringLiteral("prompt")).toString(); // Fallback
  }

case Qt::ToolTipRole:
    return tmpl.value(QStringLiteral("description")).toString();
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> TemplatesModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[SourceRole] = "source";
  roles[PromptRole] = "prompt";
  roles[AutomationModeRole] = "automationMode";
  return roles;
}

void TemplatesModel::addTemplate(const QJsonObject &tmpl) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_templates.insert(0, tmpl);
  endInsertRows();
  saveTemplates();
}

void TemplatesModel::updateTemplate(int row, const QJsonObject &tmpl) {
  if (row >= 0 && row < m_templates.size()) {
    m_templates[row] = tmpl;
    QModelIndex idx = index(row, 0);
    Q_EMIT dataChanged(idx, idx);
    saveTemplates();
  }
}

void TemplatesModel::clear() {
  beginResetModel();
  m_templates = QJsonArray();
  endResetModel();
}

void TemplatesModel::removeTemplate(int row) {
  if (row >= 0 && row < m_templates.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_templates.removeAt(row);
    endRemoveRows();
    saveTemplates();
  }
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
