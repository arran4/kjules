#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractTableModel>
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

class SessionModel : public QAbstractTableModel {
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

  enum Columns { ColName = 0, ColCount };

  explicit SessionModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setSessions(const QJsonArray &sessions);
  void addSession(const QJsonObject &session);
  void updateSession(const QJsonObject &session);
  QJsonObject getSession(int row) const;

  void loadSessions();
  void saveSessions();

private:
  QVector<SessionData> m_sessions;
  QHash<QString, int> m_idToIndex;
};

#endif // SESSIONMODEL_H
