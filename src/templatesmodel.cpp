#include "templatesmodel.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrl>
#include <QTemporaryFile>
#include <QUuid>

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
      QJsonArray sourcesArray = tmpl.value(QStringLiteral("sources")).toArray();
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
    if (!name.isEmpty())
      return name;
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

Qt::ItemFlags TemplatesModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
  if (index.isValid()) {
    return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
  } else {
    return Qt::ItemIsDropEnabled | defaultFlags;
  }
}

Qt::DropActions TemplatesModel::supportedDropActions() const {
  return Qt::CopyAction | Qt::MoveAction;
}

QStringList TemplatesModel::mimeTypes() const {
  QStringList types;
  types << QStringLiteral("application/json") << QStringLiteral("text/plain")
        << QStringLiteral("text/uri-list");
  return types;
}

QMimeData *TemplatesModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mimeData = new QMimeData();
  QJsonArray exportArray;
  for (const QModelIndex &index : indexes) {
    if (index.isValid()) {
      exportArray.append(m_templates[index.row()].toObject());
    }
  }
  QJsonDocument doc(exportArray);
  QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
  mimeData->setData(QStringLiteral("application/json"), jsonData);
  mimeData->setText(QString::fromUtf8(jsonData));

  QString tempFilePath = QDir::tempPath() + QStringLiteral("/kjules_templates_export_") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8) + QStringLiteral(".json");
  QFile tempFile(tempFilePath);
  if (tempFile.open(QIODevice::WriteOnly)) {
    tempFile.write(jsonData);
    tempFile.close();
    mimeData->setUrls(QList<QUrl>() << QUrl::fromLocalFile(tempFilePath));
  }

  return mimeData;
}

bool TemplatesModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int /*column*/,
                                  const QModelIndex &parent) {
  if (action == Qt::IgnoreAction)
    return true;

  if (!data->hasFormat(QStringLiteral("application/json")) && !data->hasText() && !data->hasUrls())
    return false;

  QByteArray jsonData;
  if (data->hasUrls()) {
    QList<QUrl> urls = data->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile()) return false;
    QFile file(urls.first().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return false;
    jsonData = file.readAll();
    file.close();
  } else if (data->hasFormat(QStringLiteral("application/json"))) {
    jsonData = data->data(QStringLiteral("application/json"));
  } else {
    jsonData = data->text().toUtf8();
  }

  QJsonDocument doc = QJsonDocument::fromJson(jsonData);
  if (!doc.isArray() && !doc.isObject())
    return false;

  int beginRow =
      (row != -1) ? row
                  : (parent.isValid() ? parent.row() : rowCount(QModelIndex()));

  if (doc.isArray()) {
    QJsonArray importArray = doc.array();
    QList<QJsonObject> objectsToInsert;
    for (int i = 0; i < importArray.size(); ++i) {
      if (importArray[i].isObject()) {
        objectsToInsert.append(importArray[i].toObject());
      }
    }
    if (objectsToInsert.isEmpty())
      return false;

    beginInsertRows(QModelIndex(), beginRow, beginRow + objectsToInsert.size() - 1);
    for (int i = 0; i < objectsToInsert.size(); ++i) {
      m_templates.insert(beginRow + i, objectsToInsert[i]);
    }
    endInsertRows();
  } else if (doc.isObject()) {
    beginInsertRows(QModelIndex(), beginRow, beginRow);
    m_templates.insert(beginRow, doc.object());
    endInsertRows();
  }

  saveTemplates();
  return true;
}
