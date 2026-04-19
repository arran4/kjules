#ifndef NEWSESSIONDIALOG_H
#define NEWSESSIONDIALOG_H

#include "sourcemodel.h"
#include <KXmlGuiWindow>
#include <QJsonObject>
#include <QSet>

class QLineEdit;
class QTextEdit;
class QListView;
class QComboBox;
class QSortFilterProxyModel;
class QCheckBox;

class QPushButton;

class SourceSelectionProxyModel;
class TemplatesModel;

class NewSessionDialog : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit NewSessionDialog(SourceModel *sourceModel,
                            TemplatesModel *templatesModel, bool hasApiKey,
                            QWidget *parent = nullptr);
  void setInitialData(const QJsonObject &data);
  void setTemplateData(const QJsonObject &data);
  void setEditMode(bool isEdit);

Q_SIGNALS:
  void createSessionRequested(const QMap<QString, QString> &sources,
                              const QString &prompt,
                              const QString &automationMode,
                              bool requirePlanApproval);
  void saveDraftRequested(const QJsonObject &draft);
  void saveTemplateRequested(const QJsonObject &tmpl);
  void loadTemplateRequested();
  void refreshSourcesRequested();

private Q_SLOTS:
  void onSubmit(const QString &automationMode);
  void onSubmitSession();
  void onLoadTemplate();
  void onSaveDraft();
  void onSaveTemplate();
  void onSelectAll();
  void onUnselectAll();
  void onAddSelected();
  void onRemoveSelected();
  void updateModels();
  QString getDefaultBranch(const QModelIndex &sourceIdx);
  void applyFilter();

protected:
  void addFavouriteAction(QMenu &menu, const QModelIndex &sourceIdx);
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  SourceModel *m_sourceModel;
  TemplatesModel *m_templatesModel;
  QListView *m_unselectedView;
  QListView *m_selectedView;
  SourceSelectionProxyModel *m_unselectedProxy;
  SourceSelectionProxyModel *m_selectedProxy;
  QLineEdit *m_filterEdit;
  QTextEdit *m_promptEdit;
  QComboBox *m_automationModeComboBox;
  QCheckBox *m_requirePlanApprovalCheckBox;
  QCheckBox *m_keepOpenCheckBox;
  QCheckBox *m_keepSourceCheckBox;
  QPushButton *m_createButton;
  QPushButton *m_loadTemplateButton;
  QPushButton *m_saveTemplateButton;
  QMap<QString, QString> m_selectedSources;
  QString m_draftComment;

  QWidget *m_sourceSelectionWidget;
};

#endif // NEWSESSIONDIALOG_H
