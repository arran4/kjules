#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QString>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    QString type = "repo";
    QString current = "NOT repo:r1";

    QRegularExpression reSingle(QStringLiteral("NOT\\s+%1:(?:\"[^\"]+\"|[^\\s\\(\\)]+)").arg(type));
    QRegularExpressionMatch matchSingle = reSingle.match(current);

    if (matchSingle.hasMatch()) {
        qDebug() << "Captured 0:" << matchSingle.captured(0);
        qDebug() << "Captured 1:" << matchSingle.captured(1);
    }
    return 0;
}
