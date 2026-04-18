#include "kjulesrunner.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>

KJulesRunner::KJulesRunner(QObject *parent, const KPluginMetaData &metaData,
                           const QVariantList &args)
    : KRunner::AbstractRunner(parent, metaData, args) {
  setObjectName(QStringLiteral("kjulesrunner"));
  addSyntax(KRunner::RunnerSyntax(
      QStringLiteral("kjules :q:"),
      i18n("Create a new kjules session with the given prompt.")));
}

void KJulesRunner::match(KRunner::RunnerContext &context) {
  QString term = context.query();
  QString prompt;
  if (term == QStringLiteral("kjules")) {
    prompt = QString();
  } else if (term.startsWith(QStringLiteral("kjules "))) {
    prompt = term.mid(7).trimmed();
  } else {
    return;
  }

  KRunner::QueryMatch match(this);
  match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
  match.setIconName(QStringLiteral("sc-apps-kjules"));
  match.setText(prompt.isEmpty()
                    ? i18n("Create new kjules session")
                    : i18n("Create new kjules session: %1", prompt));
  match.setData(prompt);
  match.setId(term);
  context.addMatch(match);
}

void KJulesRunner::run(const KRunner::RunnerContext &context,
                       const KRunner::QueryMatch &match) {
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
