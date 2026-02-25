#include <QApplication>
#include <QCommandLineParser>
#include <KAboutData>
#include <KLocalizedString>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kjules");

    KAboutData aboutData(
        QStringLiteral("kjules"),
        i18n("kJules"),
        QStringLiteral("0.1.0"),
        i18n("A frontend for Google Jules API"),
        KAboutLicense::GPL,
        i18n("(c) 2024"),
        QStringLiteral("https://developers.google.com/jules/api"),
        QStringLiteral("submit@bugs.kde.org")
    );

    aboutData.addAuthor(i18n("Jules"), i18n("Developer"), QStringLiteral("jules@kde.org"));
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    MainWindow *window = new MainWindow();
    window->show();

    return app.exec();
}
