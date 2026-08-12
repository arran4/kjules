#ifndef SOURCEREMAPDIALOG_H
#define SOURCEREMAPDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QStringList>

class QTableWidget;

struct SourceRemapEntry {
  QString location;
  QString description;
  QString source;
  int index = -1;
  QString kind;
  QJsonObject data;
  QString status;
  bool selectedByDefault = true;
};

struct SourceRemapSelection {
  SourceRemapEntry entry;
  QString newSource;
};

class SourceRemapDialog : public QDialog {
  Q_OBJECT

public:
  SourceRemapDialog(const QList<SourceRemapEntry> &entries, const QStringList &availableSources,
                    QWidget *parent = nullptr);
  QList<SourceRemapSelection> selections() const;

private:
  QList<SourceRemapEntry> m_entries;
  QTableWidget *m_table;
};

#endif // SOURCEREMAPDIALOG_H
