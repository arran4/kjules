#ifndef MIGRATIONORCHESTRATOR_H
#define MIGRATIONORCHESTRATOR_H

#include "jobstore.h"
#include "legacyconverter.h"
#include <QString>

class MigrationOrchestrator {
public:
  static bool safeMigrationSeam(const LegacyData &legacyData, const QString &destinationStorePath,
                                const QDateTime &fallbackTimestamp) {
    // 1. & 2. (Legacy data already read and supplied via arguments for testability)

    // 3. Call aggregate conversion in memory
    ConversionResult result = LegacyConverter::convertAll(legacyData, fallbackTimestamp);
    if (result.jobs.isEmpty() && result.unattachedErrors.isEmpty() && legacyData.queueItems.isEmpty() &&
        legacyData.activeSessions.isEmpty() && legacyData.archivedSessions.isEmpty() &&
        legacyData.holdingItems.isEmpty() && legacyData.errors.isEmpty()) {
      return true; // trivially empty
    }

    // 4. Write new store atomically
    JobStore tempStore(destinationStorePath);
    tempStore.setJobs(result.jobs);
    if (!tempStore.save()) {
      return false;
    }

    // 5. Reopen and validate/read back
    JobStore validationStore(destinationStorePath);
    if (!validationStore.load()) {
      return false;
    }

    // 6. Compare read-back
    if (validationStore.jobs().size() != result.jobs.size()) {
      return false;
    }

    return true;
  }
};

#endif
