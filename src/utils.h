#pragma once

#include <KLocalizedString>
#include <QString>
#include <QUrl>
#include <QDesktopServices>

namespace Utils {

inline QString formatDuration(qint64 secondsLeft) {
  if (secondsLeft >= 3600) {
    qint64 hours = secondsLeft / 3600;
    qint64 mins = (secondsLeft % 3600) / 60;
    return i18n("%1h %2m", hours, mins);
  } else if (secondsLeft >= 60) {
    qint64 mins = secondsLeft / 60;
    qint64 secs = secondsLeft % 60;
    return i18n("%1m %2s", mins, secs);
  } else {
    return i18np("1 second", "%1 seconds", secondsLeft);
  }
}

inline bool openUrl(const QUrl &url) {
  if (!url.isValid()) return false;
  const QString scheme = url.scheme().toLower();
  if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")) {
    return QDesktopServices::openUrl(url);
  }
  return false;
}

} // namespace Utils
