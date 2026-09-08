#ifndef MIGRATIONORCHESTRATOR_H
#define MIGRATIONORCHESTRATOR_H

#include "jobstore.h"
#include "legacyconverter.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QString>

class MigrationOrchestrator {
public:
  static bool &getMigratedFlag() {
    static bool isMigratedFlag = false;
    return isMigratedFlag;
  }

  static bool isMigrated() { return getMigratedFlag(); }

  static void executeMigrationIfNecessary() {
    if (getMigratedFlag())
      return;

    QString destinationPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/jobs.json");

    if (QFile::exists(destinationPath)) {
      JobStore checkStore(destinationPath);
      if (checkStore.load()) {
        getMigratedFlag() = true;
        return;
      } else {
        qWarning("Existing jobs.json is invalid/corrupt. Falling back to legacy UI sources.");
        return;
      }
    }

    // Gather legacy data
    LegacyData legacyData;

    // This is slightly tricky, we need to read from the JSON files explicitly
    // without invoking the models directly since they are tied to MainWindow lifecycle.
    auto readJsonFile = [](const QString &filename) -> QJsonDocument {
      QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + filename;
      QFile file(path);
      if (file.open(QIODevice::ReadOnly)) {
        return QJsonDocument::fromJson(file.readAll());
      }
      return QJsonDocument();
    };

    QJsonDocument queueDoc = readJsonFile(QStringLiteral("queue.json"));
    if (queueDoc.isObject() && queueDoc.object().contains(QStringLiteral("items"))) {
      for (const QJsonValue &v : queueDoc.object().value(QStringLiteral("items")).toArray()) {
        legacyData.queueItems.append(QueueItem::fromJson(v.toObject()));
      }
    }

    QJsonDocument holdingDoc = readJsonFile(QStringLiteral("holding.json"));
    if (holdingDoc.isObject() && holdingDoc.object().contains(QStringLiteral("items"))) {
      for (const QJsonValue &v : holdingDoc.object().value(QStringLiteral("items")).toArray()) {
        legacyData.holdingItems.append(QueueItem::fromJson(v.toObject()));
      }
    }

    QJsonDocument sessionsDoc = readJsonFile(QStringLiteral("cached_all_sessions.json"));
    if (sessionsDoc.isArray()) {
      for (const QJsonValue &v : sessionsDoc.array()) {
        legacyData.activeSessions.append(v.toObject());
      }
    }

    QJsonDocument errorsDoc = readJsonFile(QStringLiteral("errors.json"));
    if (errorsDoc.isArray()) {
      for (const QJsonValue &v : errorsDoc.array()) {
        legacyData.errors.append(v.toObject());
      }
    }

    QJsonDocument archivedDoc = readJsonFile(QStringLiteral("cached_archive_sessions.json"));
    if (archivedDoc.isArray()) {
      for (const QJsonValue &v : archivedDoc.array()) {
        legacyData.archivedSessions.append(v.toObject());
      }
    }

    bool success = safeMigrationSeam(legacyData, destinationPath, QDateTime::currentDateTimeUtc());
    if (success) {
      getMigratedFlag() = true;
    }
  }
  static bool safeMigrationSeam(const LegacyData &legacyData, const QString &destinationStorePath,
                                const QDateTime &fallbackTimestamp) {
    ConversionResult result = LegacyConverter::convertAll(legacyData, fallbackTimestamp);

    if (!result.unattachedErrors.isEmpty())
      return false;

    if (result.jobs.isEmpty() && legacyData.queueItems.isEmpty() && legacyData.activeSessions.isEmpty() &&
        legacyData.archivedSessions.isEmpty() && legacyData.holdingItems.isEmpty() && legacyData.errors.isEmpty()) {
      JobStore emptyStore(destinationStorePath);
      emptyStore.setJobs(result.jobs);
      if (emptyStore.save())
        return true;
      return false;
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
    for (auto &j : result.jobs)
      arr1.append(j.toJson());
    QJsonArray arr2;
    for (auto &j : validationStore.jobs())
      arr2.append(j.toJson());

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
