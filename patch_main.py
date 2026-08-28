import sys

with open("src/main.cpp", "r") as f:
    content = f.read()

content = content.replace("#include <QTemporaryDir>", "#include <QTemporaryDir>\n#include <KDBusService>")

parser_str = """
  QCommandLineOption autostartedOption(QStringList() << QStringLiteral("autostarted"), i18n("Launched via autostart"));
  parser.addOption(autostartedOption);
"""
parser_new = """
  QCommandLineOption autostartedOption(QStringList() << QStringLiteral("autostarted"), i18n("Launched via autostart"));
  parser.addOption(autostartedOption);

  QCommandLineOption newSessionOption(QStringList() << QStringLiteral("new-session"), i18n("Open the New Session dialog"));
  parser.addOption(newSessionOption);
"""
content = content.replace(parser_str, parser_new)

main_window_str = "MainWindow *window = new MainWindow();"

main_window_new = """KDBusService service(KDBusService::Unique);

  MainWindow *window = new MainWindow();

  QObject::connect(&service, &KDBusService::activateRequested, window, [window](const QStringList &arguments, const QString &/*workingDirectory*/) {
    QCommandLineParser p;
    QCommandLineOption autostartedO(QStringList() << QStringLiteral("autostarted"), i18n("Launched via autostart"));
    p.addOption(autostartedO);
    QCommandLineOption newSessionO(QStringList() << QStringLiteral("new-session"), i18n("Open the New Session dialog"));
    p.addOption(newSessionO);
    QCommandLineOption mockApiO(QStringList() << QStringLiteral("mock-api"), i18n("Use mock API at localhost:8080"));
    p.addOption(mockApiO);
    p.parse(arguments);
    if (p.isSet(newSessionO)) {
      window->showNewSessionDialogSlot();
    }
  });"""
content = content.replace(main_window_str, main_window_new)

show_str = """  if (!(isAutostarted && autostartTray)) {
    window->show();
  }"""
show_new = """  if (!(isAutostarted && autostartTray)) {
    window->show();
  }

  if (parser.isSet(newSessionOption)) {
    window->showNewSessionDialogSlot();
  }"""
content = content.replace(show_str, show_new)

with open("src/main.cpp", "w") as f:
    f.write(content)
