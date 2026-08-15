#ifndef CONFIGMIGRATION_H
#define CONFIGMIGRATION_H

#include <KConfigGroup>
#include <KSharedConfig>

class ConfigMigration {
public:
  static void migrate(const KSharedConfig::Ptr &config = KSharedConfig::openConfig()) {
    KConfigGroup sessionConfig(config, QStringLiteral("SessionWindow"));

    // 1. Remove obsolete MergeRefreshAndQueue
    if (sessionConfig.hasKey("MergeRefreshAndQueue")) {
      sessionConfig.deleteEntry("MergeRefreshAndQueue");
    }

    // 2. Migrate unsupported global FollowingAutoRefreshInterval (<= 0) to default 900
    if (sessionConfig.hasKey("FollowingAutoRefreshInterval")) {
      int val = sessionConfig.readEntry("FollowingAutoRefreshInterval", 900);
      if (val <= 0) {
        sessionConfig.writeEntry("FollowingAutoRefreshInterval", 900);
      }
    }

    sessionConfig.sync();
  }
};

#endif // CONFIGMIGRATION_H
