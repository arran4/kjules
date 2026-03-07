#ifndef NEWSESSIONDIALOG_H
#define NEWSESSIONDIALOG_H

#include "sourcemodel.h"
#include <QDialog>
#include <QJsonObject>
#include <QSet>

class QLineEdit;
class QTextEdit;
class QListView;
class QComboBox;
class QSortFilterProxyModel;

class SourceSelectionProxyModel;

class NewSessionDialog : public QDialog {
  Q_OBJECT

public:
  explicit NewSessionDialog(SourceModel *sourceModel, bool hasApiKey,
                            QWidget *parent = nullptr);
  void setInitialData(const QJsonObject &data);

Q_SIGNALS:
  void createSessionRequested(const QStringList &sources, const QString &prompt,
                              const QString &automationMode);
  void saveDraftRequested(const QJsonObject &draft);

private Q_SLOTS:
  void onSubmit(const QString &automationMode);
  void onSaveDraft();
  void onSelectAll();
  void onUnselectAll();
  void onAddSelected();
  void onRemoveSelected();
  void updateModels();

private:
  SourceModel *m_sourceModel;
  QListView *m_unselectedView;
  QListView *m_selectedView;
  SourceSelectionProxyModel *m_unselectedProxy;
  SourceSelectionProxyModel *m_selectedProxy;
  QLineEdit *m_filterEdit;
  QTextEdit *m_promptEdit;
  QSet<QString> m_selectedSources;
};

#endif // NEWSESSIONDIALOG_H
