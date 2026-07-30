#include <QString>
#include <QDebug>
#include <QVariant>
int main() {
    QString val = "false";
    qDebug() << val.contains("true", Qt::CaseInsensitive);
    return 0;
}
