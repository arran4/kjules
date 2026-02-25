#ifndef NEWSESSIONDIALOG_H
#define NEWSESSIONDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include "sourcemodel.h"

class QLineEdit;
class QTextEdit;
class QListView;
class QComboBox;

class NewSessionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewSessionDialog(SourceModel *sourceModel, QWidget *parent = nullptr);
    void setInitialData(const QJsonObject &data);

signals:
    void createSessionRequested(const QStringList &sources, const QString &prompt, const QString &automationMode);
    void saveDraftRequested(const QJsonObject &draft);

private slots:
    void onSubmit();
    void onSaveDraft();
    void onSelectAll();
    void onUnselectAll();

private:
    SourceModel *m_sourceModel;
    QListView *m_sourceView;
    QLineEdit *m_filterEdit;
    QTextEdit *m_promptEdit;
    QComboBox *m_automationModeCombo;
};

#endif // NEWSESSIONDIALOG_H
