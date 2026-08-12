#include "sourcefixer.h"

#include <QVector>
#include <algorithm>
#include <climits>
#include <tuple>

namespace {
QStringList segments(QString value) {
  if (value.startsWith(QStringLiteral("sources/"))) {
    value.remove(0, 8);
  }
  return value.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

bool isSubsequence(const QStringList &needle, const QStringList &haystack) {
  int position = 0;
  for (const QString &segment : haystack) {
    if (position < needle.size() && needle[position].compare(segment, Qt::CaseInsensitive) == 0) {
      ++position;
    }
  }
  return position == needle.size();
}

int editDistance(const QString &left, const QString &right) {
  QVector<int> previous(right.size() + 1);
  QVector<int> current(right.size() + 1);
  for (int column = 0; column <= right.size(); ++column) {
    previous[column] = column;
  }
  for (int row = 1; row <= left.size(); ++row) {
    current[0] = row;
    for (int column = 1; column <= right.size(); ++column) {
      const int substitution = left[row - 1].toCaseFolded() == right[column - 1].toCaseFolded() ? 0 : 1;
      current[column] = std::min({previous[column] + 1, current[column - 1] + 1, previous[column - 1] + substitution});
    }
    previous.swap(current);
  }
  return previous[right.size()];
}
} // namespace

namespace SourceFixer {

QString source(const QJsonObject &object) {
  const QJsonObject nested = object.value(QStringLiteral("request")).toObject();
  if (!nested.isEmpty()) {
    const QString nestedSource = source(nested);
    if (!nestedSource.isEmpty()) {
      return nestedSource;
    }
  }
  const QString flatSource = object.value(QStringLiteral("source")).toString();
  if (!flatSource.isEmpty()) {
    return flatSource;
  }
  return object.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
}

QJsonObject remap(const QJsonObject &object, const QString &newSource) {
  QJsonObject result = object;
  if (result.value(QStringLiteral("request")).isObject()) {
    result[QStringLiteral("request")] = remap(result.value(QStringLiteral("request")).toObject(), newSource);
  }
  if (result.contains(QStringLiteral("source"))) {
    result[QStringLiteral("source")] = newSource;
  }
  if (result.value(QStringLiteral("sourceContext")).isObject()) {
    QJsonObject context = result.value(QStringLiteral("sourceContext")).toObject();
    context[QStringLiteral("source")] = newSource;
    result[QStringLiteral("sourceContext")] = context;
  }
  return result;
}

QString bestMatch(const QString &oldSource, const QStringList &availableSources) {
  if (availableSources.isEmpty()) {
    return {};
  }
  const QStringList oldSegments = segments(oldSource);
  QString best;
  std::tuple<int, int, int, QString> bestScore{INT_MAX, INT_MAX, INT_MAX, QString()};
  for (const QString &candidate : availableSources) {
    const QStringList candidateSegments = segments(candidate);
    int relationship = 2;
    if (oldSource.compare(candidate, Qt::CaseInsensitive) == 0) {
      relationship = 0;
    } else if (isSubsequence(oldSegments, candidateSegments)) {
      relationship = 0; // Prefer a provider segment newly inserted by the API.
    } else if (isSubsequence(candidateSegments, oldSegments)) {
      relationship = 1;
    }
    const auto score = std::make_tuple(relationship, std::abs(candidateSegments.size() - oldSegments.size()),
                                       editDistance(oldSource, candidate), candidate);
    if (score < bestScore) {
      bestScore = score;
      best = candidate;
    }
  }
  return best;
}

bool sameSource(const QString &left, const QString &right) { return left == right; }

} // namespace SourceFixer
