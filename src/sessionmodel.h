#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

struct SessionData {
  QString id;
  QString name;
  QString title;
  QString source;
  QString prompt;
  QJsonObject rawObject;
};

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
  void clear();

private:
  QVector<SessionData> m_sessions;
  QHash<QString, int> m_idToIndex;
};

#endif // SESSIONMODEL_H
