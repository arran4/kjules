#ifndef ERRORSMODEL_H
#define ERRORSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

class ErrorsModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum ErrorRoles { RequestRole = Qt::UserRole + 1, ResponseRole, MessageRole, HttpDetailsRole };

  explicit ErrorsModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void addError(const QJsonObject &request, const QJsonObject &response,
                const QString &message, const QString &httpDetails = QString());
  void removeError(int row);
  QJsonObject getError(int row) const;
  void loadErrors();
  void saveErrors();

private:
  QJsonArray m_errors;
};

#endif // ERRORSMODEL_H
