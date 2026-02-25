#ifndef DRAFTSMODEL_H
#define DRAFTSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

class DraftsModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum DraftRoles {
    SourceRole = Qt::UserRole + 1,
    PromptRole,
    AutomationModeRole
  };

  explicit DraftsModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void addDraft(const QJsonObject &draft);
  void removeDraft(int row);
  QJsonObject getDraft(int row) const;
  void loadDrafts();
  void saveDrafts();

 private:
  QJsonArray m_drafts;
};

#endif  // DRAFTSMODEL_H
