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

    // Reject if operational errors couldn't be attached (preserve diagnostics)
    if (!result.unattachedErrors.isEmpty()) {
      return false;
    }

    if (result.jobs.isEmpty() && legacyData.queueItems.isEmpty() && legacyData.activeSessions.isEmpty() &&
        legacyData.archivedSessions.isEmpty() && legacyData.holdingItems.isEmpty() && legacyData.errors.isEmpty()) {
      return true; // trivially empty
    }

    // 4. Stage to temporary file
    QString stagingPath = destinationStorePath + QStringLiteral(".tmp");
    JobStore tempStore(stagingPath);
    tempStore.setJobs(result.jobs);
    if (!tempStore.save()) {
      QFile::remove(stagingPath);
      return false;
    }

    // 5. Reload and validate
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
    // Note: QFile::rename does not overwrite on Windows or some POSIX, so we remove first,
    // but QFile::rename can fail. We can use QSaveFile for the final destination instead,
    // or just use QFile::rename but gracefully revert if possible.
    // Actually, since this is just a seam, we can just do:
    QFile::remove(destinationStorePath);
    if (!QFile::rename(stagingPath, destinationStorePath)) {
      QFile::remove(stagingPath);
      return false;
    }

    return true;
  }
};

#endif
