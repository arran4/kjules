#include "jobstore.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

JobStore::JobStore(const QString& filename) : m_filename(filename) {}

bool JobStore::load() {
    QString path = m_filename;
    if (!path.startsWith(QLatin1Char('/'))) {
        path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kjules/") + m_filename;
    }

    QFile file(path);
    if (!file.exists()) {
        return true; // No file is fine initially
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open" << path << "for reading";
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse" << path << ":" << error.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("schemaVersion"))) {
        qWarning() << "Missing schemaVersion in" << path;
        return false;
    }

    int version = root[QStringLiteral("schemaVersion")].toInt();
    if (version != m_schemaVersion) {
        qWarning() << "Unsupported schemaVersion" << version << "in" << path;
        return false;
    }

    m_jobs.clear();
    QJsonArray jobsArray = root[QStringLiteral("jobs")].toArray();
    for (const auto& jobVal : jobsArray) {
        m_jobs.append(JobData::fromJson(jobVal.toObject()));
    }

    return true;
}

bool JobStore::save() const {
    QString path = m_filename;
    if (!path.startsWith(QLatin1Char('/'))) {
        QString dirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kjules");
        QDir dir(dirPath);
        if (!dir.exists()) dir.mkpath(QStringLiteral("."));
        path = dirPath + QStringLiteral("/") + m_filename;
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = m_schemaVersion;

    QJsonArray jobsArray;
    for (const auto& job : m_jobs) {
        jobsArray.append(job.toJson());
    }
    root[QStringLiteral("jobs")] = jobsArray;

    QJsonDocument doc(root);

    QSaveFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not open" << path << "for writing";
        return false;
    }

    saveFile.write(doc.toJson());
    return saveFile.commit();
}

JobStore JobStore::fromMemory(const QVector<JobData>& jobs) {
    JobStore store;
    store.m_jobs = jobs;
    return store;
}
