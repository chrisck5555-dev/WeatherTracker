#pragma once

#include <QDate>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QString>
#include <optional>

#include "WeatherDay.h"

// The only class that talks to SQLite. Everything else asks it for plain objects.
class Repository
{
public:
    // path = a file, or ":memory:" for a throwaway in-RAM database (used by tests).
    explicit Repository(const QString &path);
    ~Repository();
    Repository(const Repository &) = delete;              // one connection, one owner
    Repository &operator=(const Repository &) = delete;

    bool isOpen() const { return m_open; }
    QString path() const { return m_path; }
    QString lastError() const { return m_error; }

    // Where the real data file lives (outside the install folder, Requirements §5).
    static QString defaultDataPath();

    // Weather
    std::optional<WeatherDay> weatherFor(const QDate &date) const;
    QMap<QDate, WeatherDay> weatherInRange(const QDate &from, const QDate &to) const;
    bool saveWeather(const QDate &date, const WeatherDay &day);   // empty day = delete row

    // Notes (one per day)
    QString note(const QDate &date) const;
    bool saveNote(const QDate &date, const QString &text);        // blank = delete

    // Events, stored by stable key such as "first_snow"
    QSet<QString> events(const QDate &date) const;
    bool saveEvents(const QDate &date, const QSet<QString> &keys);

    // Calendar markers: a few range queries per screen, not one per cell
    QSet<QDate> datesWithNotes(const QDate &from, const QDate &to) const;
    QSet<QDate> datesWithEvents(const QDate &from, const QDate &to) const;

    // Settings (help on/off, units, text size...)
    QString setting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value);

    // Search: notes containing the text, and days that have any of the given events.
    // Newest day first, at most `limit` results.
    struct NoteHit { QDate date; QString text; };
    QList<NoteHit> searchNotes(const QString &needle, int limit = 200) const;
    QList<QDate> datesWithEventKeys(const QStringList &keys, int limit = 200) const;

    std::optional<QDate> firstWeatherDate() const;   // earliest day with a weather row

    struct Counts { int weatherDays = 0; int noteDays = 0; int eventDays = 0; };
    Counts counts() const;      // how many days have data; never the data itself

    // Backups. backupTo() writes a consistent copy of the whole database to `file`
    // (replacing it). inspect() looks inside another file WITHOUT changing it: unset when
    // it is not a Weather Tracker data file. replaceWith() swaps this database's contents
    // for that file's; on failure the old data is kept.
    bool backupTo(const QString &file) const;
    static std::optional<Counts> inspect(const QString &file);
    bool replaceWith(const QString &file);

    // Checks a data file from outside (a backup, or a copy from a USB stick) WITHOUT
    // changing it. The quick check (for lists) looks at the file type, size and tables.
    // The thorough check, used before any data is replaced, also runs SQLite's own damage
    // check and looks at every saved value.
    struct FileCheck {
        bool ok = false;
        QString problem;          // plain English for a message box; empty when ok
        Counts counts;            // only meaningful when ok
    };
    static FileCheck checkFile(const QString &file, bool thorough);
    static constexpr qint64 kMaxFileBytes = 50LL * 1024 * 1024;     // 50 MB

private:
    void createSchema();
    void applyPragmas();
    QSet<QDate> datesIn(const char *table, const QDate &from, const QDate &to) const;

    QString m_path;               // the database file (or ":memory:")
    QString m_conn;               // name of our connection inside Qt
    mutable QString m_error;      // 'mutable': even const reads may record an error
    bool m_open = false;
};