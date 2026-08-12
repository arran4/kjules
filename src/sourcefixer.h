#ifndef SOURCEFIXER_H
#define SOURCEFIXER_H

#include <QJsonObject>
#include <QStringList>

namespace SourceFixer {

QString source(const QJsonObject &object);
QJsonObject remap(const QJsonObject &object, const QString &newSource);
QString bestMatch(const QString &oldSource, const QStringList &availableSources);
bool sameSource(const QString &left, const QString &right);

} // namespace SourceFixer

#endif // SOURCEFIXER_H
