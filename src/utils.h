#pragma once

#include <KLocalizedString>
#include <QString>

namespace Utils {

inline QString formatDuration(qint64 secondsLeft) {
  if (secondsLeft > 3600) {
    qint64 hours = secondsLeft / 3600;
    qint64 mins = (secondsLeft % 3600) / 60;
    return i18n("%1h %2m", hours, mins);
  } else if (secondsLeft > 60) {
    qint64 mins = secondsLeft / 60;
    qint64 secs = secondsLeft % 60;
    return i18n("%1m %2s", mins, secs);
  } else {
    return i18np("1 second", "%1 seconds", secondsLeft);
  }
}

} // namespace Utils
