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
    ConversionResult result = LegacyConverter::convertAll(legacyData, fallbackTimestamp);

    if (!result.unattachedErrors.isEmpty()) {
      return false;
    }

    if (result.jobs.isEmpty() && legacyData.queueItems.isEmpty() && legacyData.activeSessions.isEmpty() &&
        legacyData.archivedSessions.isEmpty() && legacyData.holdingItems.isEmpty() && legacyData.errors.isEmpty()) {
      return true;
    }

    QString stagingPath = destinationStorePath + QStringLiteral(".tmp");
    JobStore tempStore(stagingPath);
    tempStore.setJobs(result.jobs);
    if (!tempStore.save()) {
      QFile::remove(stagingPath);
      return false;
    }

    JobStore validationStore(stagingPath);
    if (!validationStore.load()) {
      QFile::remove(stagingPath);
      return false;
    }

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

    JobStore finalStore(destinationStorePath);
    finalStore.setJobs(validationStore.jobs());
    if (!finalStore.save()) {
      QFile::remove(stagingPath);
      return false;
    }

    QFile::remove(stagingPath);
    return true;
  }
};

#endif
