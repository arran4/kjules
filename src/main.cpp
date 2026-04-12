#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTemporaryDir>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kjules");

  KAboutData aboutData(
      QStringLiteral("kjules"), i18n("kJules"), QStringLiteral("0.1.0"),
      i18n("A frontend for Google Jules API"), KAboutLicense::GPL,
      i18n("(c) 2024"),
      QStringLiteral("https://developers.google.com/jules/api"),
      QStringLiteral("submit@bugs.kde.org"));

  aboutData.addAuthor(i18n("Jules"), i18n("Developer"),
                      QStringLiteral("jules@kde.org"));
  KAboutData::setApplicationData(aboutData);

  QCommandLineParser parser;
  aboutData.setupCommandLine(&parser);

  QCommandLineOption autostartedOption(QStringList()
                                           << QStringLiteral("autostarted"),
                                       i18n("Launched via autostart"));
  parser.addOption(autostartedOption);

  QCommandLineOption mockApiOption(QStringList() << QStringLiteral("mock-api"),
                                   i18n("Use mock API at localhost:8080"));
  parser.addOption(mockApiOption);

  parser.process(app);
  aboutData.processCommandLine(&parser);

  bool useMockApi = parser.isSet(mockApiOption);
#ifdef USE_MOCK_API
  if (!parser.isSet(mockApiOption)) {
    useMockApi = true;
  }
#endif

  if (useMockApi) {
    QTemporaryDir *tempDir = new QTemporaryDir();
    if (tempDir->isValid()) {
      qputenv("XDG_DATA_HOME", tempDir->path().toUtf8());
    }
  }

  MainWindow *window = new MainWindow();

  bool isAutostarted = parser.isSet(autostartedOption);
  KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
  bool autostartTray = config.readEntry("AutostartTray", false);
  window->setMockApi(useMockApi);

  if (!(isAutostarted && autostartTray)) {
    window->show();
  }

  return app.exec();
}
