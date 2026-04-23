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
#ifdef DEV_MODE
  QByteArray devDataDir = QByteArray(KJULES_SOURCE_DIR) + "/.jules-dev-data";
  qputenv("XDG_DATA_HOME", devDataDir + "/data");
  qputenv("XDG_CONFIG_HOME", devDataDir + "/config");
  qputenv("XDG_CACHE_HOME", devDataDir + "/cache");
  qputenv("LOCALAPPDATA", devDataDir + "/data");
  qputenv("APPDATA", devDataDir + "/config");
#endif

  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kjules");

  KAboutData aboutData(
      QStringLiteral("org.kde.kjules"), i18n("kJules"), QStringLiteral(KJULES_VERSION),
      i18n(
          "A KDE native desktop client for tracking and managing GitHub tasks"),
      KAboutLicense::GPL, i18n("(c) 2024"),
      QStringLiteral("https://github.com/yourusername/kjules"),
      QStringLiteral("submit@bugs.kde.org"));

  aboutData.addAuthor(i18n("Jules"), i18n("Developer"),
                      QStringLiteral("jules@kde.org"));
  aboutData.setDesktopFileName(QStringLiteral("org.kde.kjules"));
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

  // If mock API is used and DEV_MODE is not active, use a temporary dir for
  // data
  if (useMockApi) {
#ifndef DEV_MODE
    QTemporaryDir *tempDir = new QTemporaryDir();
    if (tempDir->isValid()) {
      qputenv("XDG_DATA_HOME", tempDir->path().toUtf8());
      qputenv("LOCALAPPDATA", tempDir->path().toUtf8());
    }
#endif
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
