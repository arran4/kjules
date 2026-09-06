#ifndef MIGRATIONORCHESTRATOR_H
#define MIGRATIONORCHESTRATOR_H

#include "jobstore.h"
#include "legacyconverter.h"
#include <QString>

class MigrationOrchestrator {
public:
  static bool safeMigrationSeam(const LegacyData &legacyData, const QString &destinationStorePath,
                                const QDateTime &fallbackTimestamp) {
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

    // 6. Compare read-back using JSON representation
    QJsonArray arr1;
    for (auto &j : result.jobs) {
      arr1.append(j.toJson());
    }
    QJsonArray arr2;
    for (auto &j : validationStore.jobs()) {
      arr2.append(j.toJson());
    }

    auto sortArray = [](QJsonArray arr) {
      QStringList strList;
      for (const auto &v : arr) {
        QJsonDocument doc(v.toObject());
        strList.append(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
      }
      strList.sort();
      return strList;
    };

    if (sortArray(arr1) != sortArray(arr2)) {
      return false;
    }

    // Also check if we produced unattached errors but they weren't saved?
    // Wait, unattachedErrors aren't serialized to JobStore in Phase 1 currently,
    // because JobStore only stores jobs. The requirement is just "ConversionResult::unattachedErrors is not accounted
    // for by the persistence/read-back success criterion." We shouldn't fail if there are unattached errors, we just
    // need to ensure the caller knows it succeeded in writing what it *can* write. Or wait,
    // "ConversionResult::unattachedErrors is not accounted for by the persistence/read-back success criterion. Add
    // non-empty success coverage that compares the full serialized read-back Job representation, and
    // failure/non-destructive coverage... Preserve diagnostics/legacy recoverability explicitly; do not let a future
    // caller interpret a successful JobStore write as having safely migrated diagnostics that were never represented
    // there." So we should return a bool indicating if anything was lost? Or change the signature to return an object.
    // Actually, if we return true, the caller might think unattached errors were saved. Let's just return false if
    // unattached errors > 0 for the test? No, we should probably output the unattached errors or return a status
    // struct. Let's just return false if `!result.unattachedErrors.isEmpty()` for now since Phase 1 doesn't persist
    // them.
    if (!result.unattachedErrors.isEmpty()) {
      // We haven't migrated diagnostics safely yet.
      return false;
    }

    return true;
  }
};

#endif
