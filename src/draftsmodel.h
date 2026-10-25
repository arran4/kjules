#ifndef DRAFTSMODEL_H
#define DRAFTSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

class DraftsModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum DraftRoles { SourceRole = Qt::UserRole + 1, PromptRole, AutomationModeRole, CommentRole };

  explicit DraftsModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  bool addDraft(const QJsonObject &draft);
  bool updateDraft(int row, const QJsonObject &draft);
  bool removeDraft(int row);
  QJsonObject getDraft(int row) const;
  void loadDrafts();
  bool saveDrafts();
  void clear();

  void setSimulateSaveFailureForTest(bool simulate) { m_simulateSaveFailure = simulate; }

private:
  QJsonArray m_drafts;
  bool m_simulateSaveFailure = false;
};

#endif // DRAFTSMODEL_H
