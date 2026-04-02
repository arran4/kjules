#ifndef TEMPLATESMODEL_H
#define TEMPLATESMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeData>

class TemplatesModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum TemplateRoles {
    SourceRole = Qt::UserRole + 1,
    PromptRole,
    AutomationModeRole,
    NameRole,
    DescriptionRole
  };

  explicit TemplatesModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Qt::ItemFlags flags(const QModelIndex &index) const override;
  Qt::DropActions supportedDropActions() const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;

  void addTemplate(const QJsonObject &tmpl);
  void updateTemplate(int row, const QJsonObject &tmpl);
  void removeTemplate(int row);
  QJsonObject getTemplate(int row) const;
  void loadTemplates();
  void saveTemplates();
  void clear();

private:
  QJsonArray m_templates;
};

#endif // TEMPLATESMODEL_H
