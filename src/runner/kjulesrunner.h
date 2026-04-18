#ifndef KJULESRUNNER_H
#define KJULESRUNNER_H

#include <KRunner/AbstractRunner>
#include <QObject>
#include <QString>

class KJulesRunner : public KRunner::AbstractRunner {
  Q_OBJECT
public:
  explicit KJulesRunner(QObject *parent, const KPluginMetaData &metaData,
                        const QVariantList &args);
  ~KJulesRunner() override = default;

  void match(KRunner::RunnerContext &context) override;
  void run(const KRunner::RunnerContext &context,
           const KRunner::QueryMatch &match) override;
};

#endif
