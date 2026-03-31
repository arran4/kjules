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
class QCheckBox;

class SourceSelectionProxyModel;
class TemplatesModel;

class NewSessionDialog : public QDialog {
  Q_OBJECT

public:
  explicit NewSessionDialog(SourceModel *sourceModel, TemplatesModel *templatesModel, bool hasApiKey,
                            QWidget *parent = nullptr);
  void setInitialData(const QJsonObject &data);
  void setTemplateData(const QJsonObject &data);
  void setEditMode(bool isEdit);

Q_SIGNALS:
  void createSessionRequested(const QStringList &sources, const QString &prompt,
                              const QString &automationMode, bool requirePlanApproval);
  void saveDraftRequested(const QJsonObject &draft);
  void saveTemplateRequested(const QJsonObject &tmpl);
  void loadTemplateRequested();

private Q_SLOTS:
  void onSubmit(const QString &automationMode);
  void onSaveDraft();
  void onSaveTemplate();
  void onSelectAll();
  void onUnselectAll();
  void onAddSelected();
  void onRemoveSelected();
  void updateModels();
  void applyFilter();

private:
  SourceModel *m_sourceModel;
  TemplatesModel *m_templatesModel;
  QListView *m_unselectedView;
  QListView *m_selectedView;
  SourceSelectionProxyModel *m_unselectedProxy;
  SourceSelectionProxyModel *m_selectedProxy;
  QLineEdit *m_filterEdit;
  QTextEdit *m_promptEdit;
  QCheckBox *m_requirePlanApprovalCheckBox;
  QPushButton *m_createButton;
  QPushButton *m_createPRButton;
  QPushButton *m_loadTemplateButton;
  QPushButton *m_saveTemplateButton;
  QSet<QString> m_selectedSources;
  QString m_draftComment;
};

#endif // NEWSESSIONDIALOG_H
