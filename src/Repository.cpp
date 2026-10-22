#include "Repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <utility>

namespace {

QString iso(const QDate &d) { return d.toString(Qt::ISODate); }   // "2026-10-05"

QString nextConnectionName()
{
    static int counter = 0;
    return QStringLiteral("weather_%1").arg(++counter);
}

// optional/empty -> SQL NULL; otherwise the value.
QVariant dbValue(const std::optional<double> &v)
{
    if (v) return *v;
    return QVariant(QMetaType::fromType<double>());     // a typed "null"
}
QVariant dbValue(const QString &s)
{
    if (!s.isEmpty()) return s;
    return QVariant(QMetaType::fromType<QString>());
}

std::optional<double> readDouble(const QVariant &v)
{
    if (v.isNull()) return std::nullopt;                // NULL stays "not recorded"
    return v.toDouble();
}

} // namespace

Repository::Repository(const QString &path)
    : m_path(path)
    , m_conn(nextConnectionName())
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_conn);
    db.setDatabaseName(path);
    if (!db.open()) {
        m_error = db.lastError().text();
        return;
    }
    createSchema();
}

Repository::~Repository()
{
    {   // scope: the handle must be gone before removeDatabase
        QSqlDatabase db = QSqlDatabase::database(m_conn, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_conn);
}

QString Repository::defaultDataPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);                                 // create the folder if missing
    return dir + "/weather.db";
}

void Repository::createSchema()
{
    const char *statements[] = {
        "CREATE TABLE IF NOT EXISTS weather ("
        " date TEXT PRIMARY KEY, high_c REAL, low_c REAL, wind_kmh REAL, wind_dir TEXT,"
        " precip_type TEXT, precip_mm REAL, humidity REAL, pressure_hpa REAL, cloud TEXT)",

        "CREATE TABLE IF NOT EXISTS notes (date TEXT PRIMARY KEY, text TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS events ("
        " date TEXT NOT NULL, key TEXT NOT NULL, PRIMARY KEY (date, key))",

        "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
    };
    for (const char *sql : statements) {
        QSqlQuery q(QSqlDatabase::database(m_conn, false));
        if (!q.exec(QString::fromUtf8(sql))) {
            m_error = q.lastError().text();
            return;
        }
    }

    // Upgrade older data files: add the gust columns if they are missing.
    QSet<QString> columns;
    {
        QSqlQuery info(QSqlDatabase::database(m_conn, false));
        if (info.exec("PRAGMA table_info(weather)"))
            while (info.next())
                columns.insert(info.value(1).toString());
    }
    const std::pair<const char *, const char *> added[] = {
        {"gust_kmh", "ALTER TABLE weather ADD COLUMN gust_kmh REAL"},
        {"gust_dir", "ALTER TABLE weather ADD COLUMN gust_dir TEXT"},
    };
    for (const auto &[name, sql] : added) {
        if (columns.contains(QString::fromLatin1(name)))
            continue;
        QSqlQuery q(QSqlDatabase::database(m_conn, false));
        if (!q.exec(QString::fromUtf8(sql))) {
            m_error = q.lastError().text();
            return;
        }
    }
    m_open = true;
}

// ---------------- Weather ----------------

QMap<QDate, WeatherDay> Repository::weatherInRange(const QDate &from, const QDate &to) const
{
    QMap<QDate, WeatherDay> result;
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare("SELECT date, high_c, low_c, wind_kmh, wind_dir, precip_type, precip_mm,"
              " humidity, pressure_hpa, cloud, gust_kmh, gust_dir FROM weather"
              " WHERE date BETWEEN ? AND ? ORDER BY date");
    q.addBindValue(iso(from));
    q.addBindValue(iso(to));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return result;
    }
    while (q.next()) {
        WeatherDay d;
        d.highC       = readDouble(q.value(1));
        d.lowC        = readDouble(q.value(2));
        d.windKmh     = readDouble(q.value(3));
        d.windDir     = q.value(4).toString();
        d.precipType  = q.value(5).toString();
        d.precipMm    = readDouble(q.value(6));
        d.humidityPct = readDouble(q.value(7));
        d.pressureHpa = readDouble(q.value(8));
        d.cloud       = q.value(9).toString();
        d.gustKmh     = readDouble(q.value(10));
        d.gustDir     = q.value(11).toString();
        result.insert(QDate::fromString(q.value(0).toString(), Qt::ISODate), d);
    }
    return result;
}

std::optional<WeatherDay> Repository::weatherFor(const QDate &date) const
{
    const auto rows = weatherInRange(date, date);
    const auto it = rows.constFind(date);
    if (it == rows.constEnd())
        return std::nullopt;
    return it.value();
}

bool Repository::saveWeather(const QDate &date, const WeatherDay &day)
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    if (day.isEmpty()) {                                // nothing recorded: no row at all
        q.prepare("DELETE FROM weather WHERE date = ?");
        q.addBindValue(iso(date));
    } else {
        q.prepare("INSERT OR REPLACE INTO weather (date, high_c, low_c, wind_kmh, wind_dir,"
                  " precip_type, precip_mm, humidity, pressure_hpa, cloud, gust_kmh, gust_dir)"
                  " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(iso(date));
        q.addBindValue(dbValue(day.highC));
        q.addBindValue(dbValue(day.lowC));
        q.addBindValue(dbValue(day.windKmh));
        q.addBindValue(dbValue(day.windDir));
        q.addBindValue(dbValue(day.precipType));
        q.addBindValue(dbValue(day.precipMm));
        q.addBindValue(dbValue(day.humidityPct));
        q.addBindValue(dbValue(day.pressureHpa));
        q.addBindValue(dbValue(day.cloud));
        q.addBindValue(dbValue(day.gustKmh));
        q.addBindValue(dbValue(day.gustDir));
    }
    if (q.exec())
        return true;
    m_error = q.lastError().text();
    return false;
}

// ---------------- Notes ----------------

QString Repository::note(const QDate &date) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare("SELECT text FROM notes WHERE date = ?");
    q.addBindValue(iso(date));
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

bool Repository::saveNote(const QDate &date, const QString &text)
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    if (text.trimmed().isEmpty()) {
        q.prepare("DELETE FROM notes WHERE date = ?");
        q.addBindValue(iso(date));
    } else {
        q.prepare("INSERT OR REPLACE INTO notes (date, text) VALUES (?, ?)");
        q.addBindValue(iso(date));
        q.addBindValue(text);
    }
    if (q.exec())
        return true;
    m_error = q.lastError().text();
    return false;
}

// ---------------- Events ----------------

QSet<QString> Repository::events(const QDate &date) const
{
    QSet<QString> keys;
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare("SELECT key FROM events WHERE date = ?");
    q.addBindValue(iso(date));
    if (q.exec())
        while (q.next())
            keys.insert(q.value(0).toString());
    return keys;
}

bool Repository::saveEvents(const QDate &date, const QSet<QString> &keys)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    if (!db.transaction()) {
        m_error = db.lastError().text();
        return false;
    }
    QSqlQuery del(db);
    del.prepare("DELETE FROM events WHERE date = ?");
    del.addBindValue(iso(date));
    bool ok = del.exec();

    for (const QString &key : keys) {
        if (!ok) break;
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO events (date, key) VALUES (?, ?)");
        ins.addBindValue(iso(date));
        ins.addBindValue(key);
        ok = ins.exec();
        if (!ok) m_error = ins.lastError().text();
    }
    if (!ok) {
        if (m_error.isEmpty()) m_error = del.lastError().text();
        db.rollback();                                  // undo everything from this save
        return false;
    }
    if (!db.commit()) {
        m_error = db.lastError().text();
        return false;
    }
    return true;
}

// ---------------- Calendar markers ----------------

QSet<QDate> Repository::datesIn(const char *table, const QDate &from, const QDate &to) const
{
    QSet<QDate> dates;
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    // 'table' is one of our own constants, never user input, so building the
    // text here is safe. Values (the dates) always go through '?' placeholders.
    q.prepare(QString("SELECT DISTINCT date FROM %1 WHERE date BETWEEN ? AND ?")
                  .arg(QLatin1String(table)));
    q.addBindValue(iso(from));
    q.addBindValue(iso(to));
    if (q.exec())
        while (q.next())
            dates.insert(QDate::fromString(q.value(0).toString(), Qt::ISODate));
    return dates;
}

QSet<QDate> Repository::datesWithNotes(const QDate &from, const QDate &to) const
{
    return datesIn("notes", from, to);
}

QSet<QDate> Repository::datesWithEvents(const QDate &from, const QDate &to) const
{
    return datesIn("events", from, to);
}

// ---------------- Settings ----------------

QString Repository::setting(const QString &key, const QString &defaultValue) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

bool Repository::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    if (q.exec())
        return true;
    m_error = q.lastError().text();
    return false;
}

QList<Repository::NoteHit> Repository::searchNotes(const QString &needle, int limit) const
{
    QList<NoteHit> hits;
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    // instr() looks for the text itself, so "%" or "_" typed by the user are just characters.
    q.prepare("SELECT date, text FROM notes WHERE instr(lower(text), lower(?)) > 0"
              " ORDER BY date DESC LIMIT ?");
    q.addBindValue(needle);
    q.addBindValue(limit);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return hits;
    }
    while (q.next())
        hits.append({QDate::fromString(q.value(0).toString(), Qt::ISODate),
                     q.value(1).toString()});
    return hits;
}

QList<QDate> Repository::datesWithEventKeys(const QStringList &keys, int limit) const
{
    QList<QDate> dates;
    if (keys.isEmpty())
        return dates;
    const QStringList marks(keys.size(), QStringLiteral("?"));
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.prepare(QString("SELECT DISTINCT date FROM events WHERE key IN (%1)"
                      " ORDER BY date DESC LIMIT ?").arg(marks.join(",")));
    for (const QString &key : keys)
        q.addBindValue(key);
    q.addBindValue(limit);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return dates;
    }
    while (q.next())
        dates.append(QDate::fromString(q.value(0).toString(), Qt::ISODate));
    return dates;
}

std::optional<QDate> Repository::firstWeatherDate() const
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    if (q.exec("SELECT MIN(date) FROM weather") && q.next() && !q.value(0).isNull())
        return QDate::fromString(q.value(0).toString(), Qt::ISODate);
    return std::nullopt;          // MIN of an empty table is NULL
}

bool Repository::backupTo(const QString &file) const
{
    QFile::remove(file);                                // VACUUM INTO needs a file that is not there
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    QString quoted = file;
    quoted.replace('\'', "''");                         // the path goes inside SQL quotes
    if (q.exec(QString("VACUUM INTO '%1'").arg(quoted)))
        return true;
    m_error = q.lastError().text();
    return false;
}

std::optional<Repository::Counts> Repository::inspect(const QString &file)
{
    if (!QFileInfo::exists(file))
        return std::nullopt;
    const QString name = nextConnectionName();
    std::optional<Counts> result;
    {   // scope: the handle must be gone before removeDatabase
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(file);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");   // never changes the file
        if (db.open()) {
            auto count = [&db](const char *sql) {
                QSqlQuery q(db);
                if (q.exec(QString::fromUtf8(sql)) && q.next())
                    return q.value(0).toInt();
                return -1;                              // for example: no such table
            };
            const int weather = count("SELECT COUNT(*) FROM weather");
            if (weather >= 0) {                         // a weather table: it is ours
                Counts c;
                c.weatherDays = weather;
                c.noteDays = qMax(0, count("SELECT COUNT(*) FROM notes"));
                c.eventDays = qMax(0, count("SELECT COUNT(DISTINCT date) FROM events"));
                result = c;
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return result;
}

bool Repository::replaceWith(const QString &file)
{
    if (!inspect(file)) {
        m_error = "That file is not Weather Tracker data.";
        return false;
    }
    if (m_path == ":memory:" || m_path.isEmpty()) {
        m_error = "This database is not stored in a file.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    db.close();                                         // Windows cannot replace an open file

    const QString temp = m_path + ".restoring";
    QFile::remove(temp);
    bool ok = QFile::copy(file, temp);                  // copy first: the old data is still safe
    if (ok)
        ok = QFile::remove(m_path) && QFile::rename(temp, m_path);
    if (!ok) {
        m_error = "Could not replace the data file.";
        QFile::remove(temp);
    }

    m_open = false;
    if (!db.open()) {                                   // reopen whichever file is there now
        m_error = db.lastError().text();
        return false;
    }
    createSchema();                                     // also upgrades an older backup
    return ok && m_open;
}

Repository::Counts Repository::counts() const
{
    auto count = [this](const char *sql) {
        QSqlQuery q(QSqlDatabase::database(m_conn, false));
        if (q.exec(QString::fromUtf8(sql)) && q.next())
            return q.value(0).toInt();
        return 0;
    };
    Counts c;
    c.weatherDays = count("SELECT COUNT(*) FROM weather");
    c.noteDays = count("SELECT COUNT(*) FROM notes");
    c.eventDays = count("SELECT COUNT(DISTINCT date) FROM events");
    return c;
}