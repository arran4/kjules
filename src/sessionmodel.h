#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

class SessionModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum SessionRoles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    TitleRole,
    SourceRole,
    PromptRole,
    StatusRole
  };

  explicit SessionModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setSessions(const QJsonArray &sessions);
  void addSession(const QJsonObject &session);
  void updateSession(const QJsonObject &session);
  QJsonObject getSession(int row) const;

private:
  QJsonArray m_sessions;
};

#endif // SESSIONMODEL_H
