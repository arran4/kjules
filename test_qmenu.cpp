#include <QMenu>
#include <QApplication>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QMenu menu;
    bool b = menu.isEmpty();
    return b ? 0 : 1;
}
