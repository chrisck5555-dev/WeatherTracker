#include "Backups.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace Backups {

namespace {

const QString kDailyPattern = QStringLiteral("weather-????-??-??.db");
const QString kSafetyPattern = QStringLiteral("weather-before-restore-*.db");

// Keeps only the newest `keep` files that match `pattern` (the names sort by date).
void prune(const QString &folder, const QString &pattern, int keep)
{
    const QDir dir(folder);
    const QStringList names = dir.entryList({pattern}, QDir::Files, QDir::Name | QDir::Reversed);
    for (int i = keep; i < names.size(); ++i)
        QFile::remove(dir.filePath(names[i]));
}

bool copyViaTemp(const Repository &repo, const QString &target, QString *error)
{
    const QString temp = target + ".tmp";
    if (!repo.backupTo(temp)) {                 // a failed copy never damages the old file
        if (error) *error = repo.lastError();
        QFile::remove(temp);
        return false;
    }
    QFile::remove(target);
    if (!QFile::rename(temp, target)) {
        if (error) *error = "Could not write " + target;
        QFile::remove(temp);
        return false;
    }
    return true;
}

} // namespace

QString defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + "/Weather Tracker backups";
}

bool autoBackupAllowed()
{
    // WT_DB points the program at some other file (demo data). Backing that up would
    // overwrite the real backup of the same day.
    return !qEnvironmentVariableIsSet("WT_DB");
}

QString pathFor(const QDate &day, const QString &folder)
{
    return folder + "/weather-" + day.toString("yyyy-MM-dd") + ".db";
}

bool existsFor(const QDate &day, const QString &folder)
{
    return QFileInfo::exists(pathFor(day, folder));
}

bool backupNow(const Repository &repo, QString *error, const QString &folder, const QDate &day)
{
    if (!QDir().mkpath(folder)) {
        if (error) *error = "Could not create the folder " + folder;
        return false;
    }
    if (!copyViaTemp(repo, pathFor(day, folder), error))
        return false;
    prune(folder, kDailyPattern, kKeep);
    return true;
}

QString safetyCopy(const Repository &repo, QString *error, const QString &folder)
{
    if (!QDir().mkpath(folder)) {
        if (error) *error = "Could not create the folder " + folder;
        return QString();
    }
    const QString target = folder + "/weather-before-restore-"
                           + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".db";
    if (!copyViaTemp(repo, target, error))
        return QString();
    prune(folder, kSafetyPattern, kKeepSafety);
    return target;
}

QList<Entry> list(const QString &folder)
{
    QList<Entry> entries;
    const QDir dir(folder);
    const QStringList names = dir.entryList({kDailyPattern}, QDir::Files, QDir::Name | QDir::Reversed);
    for (const QString &name : names) {
        Entry e;
        e.path = dir.filePath(name);
        e.day = QDate::fromString(name.mid(8, 10), "yyyy-MM-dd");
        e.saved = QFileInfo(e.path).lastModified();
        const auto counts = Repository::inspect(e.path);
        if (!e.day.isValid() || !counts)
            continue;                           // not one of ours, or damaged: do not offer it
        e.counts = *counts;
        entries << e;
    }
    return entries;
}

} // namespace Backups
