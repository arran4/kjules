#include <QVariant>
#include <QJsonObject>

void test() {
    QVariant v = QJsonObject();
    QJsonObject obj = v.toJsonObject();
}
