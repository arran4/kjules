#include <QCoreApplication>
#include <KDBusService>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    KDBusService service(KDBusService::Unique);
    return 0;
}
