#ifndef KJULESRUNNER_H
#define KJULESRUNNER_H

#include <KRunner/AbstractRunner>
#include <QObject>
#include <QString>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class KJulesRunner : public KRunner::AbstractRunner {
#else
class KJulesRunner : public Plasma::AbstractRunner {
#endif
    Q_OBJECT
public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    explicit KJulesRunner(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args);
#else
    explicit KJulesRunner(QObject *parent, const QVariantList &args);
#endif
    ~KJulesRunner() override = default;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
#else
    void match(Plasma::RunnerContext &context) override;
    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match) override;
#endif
};

#endif
