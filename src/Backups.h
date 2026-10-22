#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

#include "Repository.h"

// Safety copies of the weather data. One file per day ("weather-2026-10-21.db"):
// a later backup on the same day replaces the earlier one, and only the newest
// kKeep days are kept. No GUI.
namespace Backups {

inline constexpr int kKeep = 10;           // how many daily backups to keep
inline constexpr int kKeepSafety = 3;      // how many "before restore" copies to keep

QString defaultFolder();                   // Documents\\Weather Tracker backups
bool autoBackupAllowed();                  // false when WT_DB is set (developer's demo data)

QString pathFor(const QDate &day, const QString &folder = defaultFolder());
bool existsFor(const QDate &day, const QString &folder = defaultFolder());

// Writes today's backup (replacing one already made today) and removes the oldest.
bool backupNow(const Repository &repo, QString *error = nullptr,
               const QString &folder = defaultFolder(),
               const QDate &day = QDate::currentDate());

// A copy of the current data, made just before it is replaced. Returns its path, or "".
QString safetyCopy(const Repository &repo, QString *error = nullptr,
                   const QString &folder = defaultFolder());

struct Entry {
    QString path;
    QDate day;
    QDateTime saved;                       // when the file was written
    Repository::Counts counts;             // what is inside
};
QList<Entry> list(const QString &folder = defaultFolder());      // newest first

} // namespace Backups
