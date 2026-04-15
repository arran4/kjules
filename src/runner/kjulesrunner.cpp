#include "kjulesrunner.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
KJulesRunner::KJulesRunner(QObject *parent, const KPluginMetaData &metaData,
                           const QVariantList &args)
    : KRunner::AbstractRunner(parent, metaData, args) {
#else
KJulesRunner::KJulesRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args) {
#endif
  setObjectName(QStringLiteral("kjulesrunner"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  addSyntax(KRunner::RunnerSyntax(
      QStringLiteral("kjules :q:"),
      i18n("Create a new kjules session with the given prompt.")));
#else
  addSyntax(Plasma::RunnerSyntax(
      QStringLiteral("kjules :q:"),
      i18n("Create a new kjules session with the given prompt.")));
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void KJulesRunner::match(KRunner::RunnerContext &context) {
#else
void KJulesRunner::match(Plasma::RunnerContext &context) {
#endif
  QString term = context.query();
  if (!term.startsWith(QStringLiteral("kjules "))) {
    return;
  }

  QString prompt = term.mid(7).trimmed();
  if (prompt.isEmpty()) {
    return;
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  KRunner::QueryMatch match(this);
  match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
#else
  Plasma::QueryMatch match(this);
  match.setCategoryRelevance(Plasma::QueryMatch::CategoryRelevance::Highest);
#endif
  match.setIconName(QStringLiteral("sc-apps-kjules"));
  match.setText(i18n("Create new kjules session: %1", prompt));
  match.setData(prompt);
  match.setId(term);
  context.addMatch(match);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void KJulesRunner::run(const KRunner::RunnerContext &context,
                       const KRunner::QueryMatch &match) {
#else
void KJulesRunner::run(const Plasma::RunnerContext &context,
                       const Plasma::QueryMatch &match) {
#endif
  Q_UNUSED(context);
  QString prompt = match.data().toString();
  QDBusMessage msg = QDBusMessage::createMethodCall(
      QStringLiteral("org.kde.kjules"), QStringLiteral("/"),
      QStringLiteral("org.kde.kjules"),
      QStringLiteral("showNewSessionDialogWithPrompt"));
  msg << prompt;
  QDBusConnection::sessionBus().send(msg);
}

K_PLUGIN_CLASS_WITH_JSON(KJulesRunner, "plasma-runner-kjules.json")

#include "kjulesrunner.moc"
