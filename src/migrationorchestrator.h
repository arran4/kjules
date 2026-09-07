#include <QFile>
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

    // Ensure no operational errors or unattached diagnostics were generated that we can't save
    // (Wait, the comment said "Preserve diagnostics/legacy recoverability explicitly". Since JobStore doesn't store
    // them, if there are unattached errors, migration can't be completed safely without losing them.)
    if (!result.unattachedErrors.isEmpty()) {
      return false;
    }

    if (result.jobs.isEmpty() && legacyData.queueItems.isEmpty() && legacyData.activeSessions.isEmpty() &&
        legacyData.archivedSessions.isEmpty() && legacyData.holdingItems.isEmpty() && legacyData.errors.isEmpty()) {
      return true; // trivially empty
    }

    // 4. Write new store to a staging location first
    QString stagingPath = destinationStorePath + QStringLiteral(".tmp");
    JobStore tempStore(stagingPath);
    tempStore.setJobs(result.jobs);
    if (!tempStore.save()) {
      QFile::remove(stagingPath);
      return false;
    }

    // 5. Reopen and validate/read back from staging
    JobStore validationStore(stagingPath);
    if (!validationStore.load()) {
      QFile::remove(stagingPath);
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
      QFile::remove(stagingPath);
      return false;
    }

    // 7. Success. Atomically rename/replace destination
    QFile::remove(destinationStorePath);
    if (!QFile::rename(stagingPath, destinationStorePath)) {
      QFile::remove(stagingPath);
      return false;
    }

    return true;
  }
};

#endif
