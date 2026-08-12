#include "../src/sourceremapdialog.h"

#include <QPushButton>
#include <QTableWidget>
#include <QtTest>

class SourceRemapDialogTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void applyCheckedDismissesAndHonoursSelection();
};

void SourceRemapDialogTest::applyCheckedDismissesAndHonoursSelection() {
  const QList<SourceRemapEntry> entries{
      {QStringLiteral("Errors"), QStringLiteral("First"), QStringLiteral("sources/arran4/blog"), 0,
       QStringLiteral("error"), QJsonObject{{QStringLiteral("prompt"), QStringLiteral("First")}},
       QStringLiteral("Orphaned"), true},
      {QStringLiteral("Queue"), QStringLiteral("Second"), QStringLiteral("sources/arran4/blog"), 1,
       QStringLiteral("queue"), QJsonObject{{QStringLiteral("prompt"), QStringLiteral("Second")}},
       QStringLiteral("Orphaned"), true}};
  SourceRemapDialog dialog(entries, {QStringLiteral("sources/github/arran4/blog")});
  QTableWidget *table = dialog.findChild<QTableWidget *>();
  QVERIFY(table);
  QCOMPARE(table->item(0, 0)->checkState(), Qt::Checked);
  QCOMPARE(table->item(1, 0)->checkState(), Qt::Checked);
  table->item(1, 0)->setCheckState(Qt::Unchecked);

  const QList<SourceRemapSelection> selections = dialog.selections();
  QCOMPARE(selections.size(), 1);
  QCOMPARE(selections.first().entry.description, QStringLiteral("First"));

  QPushButton *apply = nullptr;
  for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
    if (button->text() == QStringLiteral("Apply Checked")) {
      apply = button;
      break;
    }
  }
  QVERIFY(apply);
  QTest::mouseClick(apply, Qt::LeftButton);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
}

QTEST_MAIN(SourceRemapDialogTest)
#include "test_sourceremapdialog.moc"
