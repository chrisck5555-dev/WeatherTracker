#include "Repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <cmath>
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

// Settings that apply to every connection to our own file.
void Repository::applyPragmas()
{
    QSqlQuery q(QSqlDatabase::database(m_conn, false));
    q.exec("PRAGMA trusted_schema = OFF");              // never run code stored in the file
    q.exec("PRAGMA secure_delete = ON");                // deleted notes are wiped, not just unlinked
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
    applyPragmas();                                     // runs on every (re)open
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

// ---------------- Deleting ----------------

bool Repository::deleteDay(const QDate &date)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    if (!db.transaction()) {
        m_error = db.lastError().text();
        return false;
    }
    bool ok = true;
    for (const char *sql : {"DELETE FROM weather WHERE date = ?",
                            "DELETE FROM notes WHERE date = ?",
                            "DELETE FROM events WHERE date = ?"}) {
        QSqlQuery q(db);
        q.prepare(QString::fromLatin1(sql));
        q.addBindValue(iso(date));
        if (!q.exec()) {
            m_error = q.lastError().text();
            ok = false;
            break;
        }
    }
    if (ok && db.commit())
        return true;                                    // all three, or none
    if (ok)
        m_error = db.lastError().text();
    db.rollback();
    return false;
}

bool Repository::eraseAll()
{
    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    if (!db.transaction()) {
        m_error = db.lastError().text();
        return false;
    }
    bool ok = true;
    for (const char *sql : {"DELETE FROM weather", "DELETE FROM notes", "DELETE FROM events"}) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromLatin1(sql))) {
            m_error = q.lastError().text();
            ok = false;
            break;
        }
    }
    if (!ok || !db.commit()) {
        if (ok)
            m_error = db.lastError().text();
        db.rollback();
        return false;
    }
    // Rebuild the file so nothing erased is left in its unused space. (secure_delete
    // already blanks it; VACUUM also makes the file small again.) A failed VACUUM leaves
    // the data erased, so it is not an error.
    QSqlQuery vacuum(db);
    vacuum.exec("VACUUM");
    return true;
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

// ---------------- Checking files from outside ----------------
//
// A backup or a copy on a USB stick could be damaged, or could have been edited by some
// other program. Nothing from such a file is used until it has passed these checks.

namespace {

// What the program itself can store. Anything outside this means the file was damaged or
// edited elsewhere. The limits mirror the editor's own (wind, gust and precipitation are
// never negative; humidity is 0-100), plus size caps no real value comes near.
constexpr double kMaxMagnitude = 1e6;
constexpr int kMaxShortText = 100;            // dropdown choices and event keys
constexpr int kMaxNoteChars = 1000000;
constexpr int kMaxSettingChars = 100000;

const QString kNotOurs = QStringLiteral("That file is not Weather Tracker data.");

bool hasSqliteHeader(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(16) == QByteArray("SQLite format 3", 16);   // 15 letters + the ending 0
}

bool isNumberOrNull(const QVariant &v, double lo, double hi)
{
    if (v.isNull())
        return true;
    const int type = v.typeId();
    if (type != QMetaType::Double && type != QMetaType::LongLong && type != QMetaType::Int)
        return false;                                     // text or binary where a number belongs
    const double d = v.toDouble();
    return std::isfinite(d) && d >= lo && d <= hi;
}

bool isTextOrNull(const QVariant &v, int maxChars)
{
    if (v.isNull())
        return true;
    return v.typeId() == QMetaType::QString && v.toString().size() <= maxChars;
}

bool isText(const QVariant &v, int maxChars)
{
    return !v.isNull() && isTextOrNull(v, maxChars);
}

// A day exactly as the program writes it: "2026-10-05".
bool isDay(const QVariant &v)
{
    if (v.isNull() || v.typeId() != QMetaType::QString)
        return false;
    const QString s = v.toString();
    const QDate d = QDate::fromString(s, Qt::ISODate);
    return d.isValid() && d.toString(Qt::ISODate) == s && d.year() >= 1800 && d.year() <= 2200;
}

QString dayWords(const QVariant &v)
{
    const QDate d = QDate::fromString(v.toString(), Qt::ISODate);
    return d.isValid() ? d.toString("MMMM d, yyyy") : QStringLiteral("a day with an unreadable date");
}

QString badValue(const QString &where)
{
    return QString("That file has a value Weather Tracker could not have saved, in %1.").arg(where);
}

// Column name -> part of the primary key (0 = not part of it), as PRAGMA table_info reports.
using Columns = QMap<QString, int>;

Columns columnsOf(QSqlDatabase &db, const QString &table)
{
    Columns columns;
    QSqlQuery q(db);
    if (q.exec(QString("PRAGMA table_info(%1)").arg(table)))   // `table` is one of ours
        while (q.next())
            columns.insert(q.value(1).toString(), q.value(5).toInt());
    return columns;
}

// Every column the program needs must be there, with the same primary key. Newer versions
// may add columns, so extra ones are allowed.
bool hasColumns(const Columns &have, const Columns &need)
{
    for (auto it = need.constBegin(); it != need.constEnd(); ++it)
        if (!have.contains(it.key()) || have.value(it.key()) != it.value())
            return false;
    return true;
}

// Looks at every saved value. Returns the problem, or an empty string.
QString checkValues(QSqlDatabase &db, const QSet<QString> &tables, const Columns &weatherColumns)
{
    {
        const bool gust = weatherColumns.contains("gust_kmh") && weatherColumns.contains("gust_dir");
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QString("SELECT date, high_c, low_c, wind_kmh, wind_dir, precip_type, precip_mm,"
                            " humidity, pressure_hpa, cloud%1 FROM weather")
                        .arg(gust ? ", gust_kmh, gust_dir" : "")))
            return kNotOurs;
        while (q.next()) {
            const QVariant date = q.value(0);
            const bool ok = isDay(date)
                && isNumberOrNull(q.value(1), -kMaxMagnitude, kMaxMagnitude)    // high
                && isNumberOrNull(q.value(2), -kMaxMagnitude, kMaxMagnitude)    // low
                && isNumberOrNull(q.value(3), 0, kMaxMagnitude)                 // wind
                && isTextOrNull(q.value(4), kMaxShortText)                      // wind direction
                && isTextOrNull(q.value(5), kMaxShortText)                      // precipitation type
                && isNumberOrNull(q.value(6), 0, kMaxMagnitude)                 // precipitation
                && isNumberOrNull(q.value(7), 0, 100)                           // humidity
                && isNumberOrNull(q.value(8), -kMaxMagnitude, kMaxMagnitude)    // pressure
                && isTextOrNull(q.value(9), kMaxShortText)                      // cloud cover
                && (!gust || (isNumberOrNull(q.value(10), 0, kMaxMagnitude)     // gust
                              && isTextOrNull(q.value(11), kMaxShortText)));    // gust direction
            if (!ok)
                return badValue("the weather for " + dayWords(date));
        }
    }
    if (tables.contains("notes")) {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec("SELECT date, text FROM notes"))
            return kNotOurs;
        while (q.next())
            if (!isDay(q.value(0)) || !isText(q.value(1), kMaxNoteChars))
                return badValue("the note for " + dayWords(q.value(0)));
    }
    if (tables.contains("events")) {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec("SELECT date, key FROM events"))
            return kNotOurs;
        while (q.next())
            if (!isDay(q.value(0)) || !isText(q.value(1), kMaxShortText))
                return badValue("the events for " + dayWords(q.value(0)));
    }
    if (tables.contains("settings")) {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec("SELECT key, value FROM settings"))
            return kNotOurs;
        while (q.next())
            if (!isText(q.value(0), kMaxShortText) || !isText(q.value(1), kMaxSettingChars))
                return badValue("the settings");
    }
    return QString();
}

} // namespace

Repository::FileCheck Repository::checkFile(const QString &file, bool thorough)
{
    FileCheck result;
    const QFileInfo info(file);
    if (!info.exists() || !info.isFile() || !hasSqliteHeader(file)) {
        result.problem = kNotOurs;
        return result;
    }
    if (info.size() > kMaxFileBytes) {
        result.problem = "That file is too large to be Weather Tracker data.";
        return result;
    }

    const QString name = nextConnectionName();
    {   // scope: the handle must be gone before removeDatabase
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(file);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");   // never changes the file
        result.problem = [&]() -> QString {
            if (!db.open())
                return kNotOurs;
            QSqlQuery pragma(db);
            pragma.exec("PRAGMA trusted_schema = OFF");  // do not run anything the file defines

            // What is inside: our tables, and nothing that runs code by itself.
            QSet<QString> tables;
            {
                QSqlQuery q(db);
                if (!q.exec("SELECT type, name FROM sqlite_master"))
                    return "That file is damaged.";
                while (q.next()) {
                    const QString type = q.value(0).toString();
                    if (type == "trigger" || type == "view")
                        return "That file contains parts that Weather Tracker never makes.";
                    if (type == "table")
                        tables.insert(q.value(1).toString());
                }
            }
            if (!tables.contains("weather"))
                return kNotOurs;

            const Columns weather = columnsOf(db, "weather");
            const bool shapeOk =
                hasColumns(weather, {{"date", 1}, {"high_c", 0}, {"low_c", 0}, {"wind_kmh", 0},
                                     {"wind_dir", 0}, {"precip_type", 0}, {"precip_mm", 0},
                                     {"humidity", 0}, {"pressure_hpa", 0}, {"cloud", 0}})
                && (!tables.contains("notes")
                    || hasColumns(columnsOf(db, "notes"), {{"date", 1}, {"text", 0}}))
                && (!tables.contains("events")
                    || hasColumns(columnsOf(db, "events"), {{"date", 1}, {"key", 2}}))
                && (!tables.contains("settings")
                    || hasColumns(columnsOf(db, "settings"), {{"key", 1}, {"value", 0}}));
            if (!shapeOk)
                return kNotOurs;

            if (thorough) {
                QSqlQuery q(db);
                if (!q.exec("PRAGMA integrity_check") || !q.next() || q.value(0).toString() != "ok")
                    return "That file is damaged.";
                const QString valueProblem = checkValues(db, tables, weather);
                if (!valueProblem.isEmpty())
                    return valueProblem;
            }

            auto count = [&db](const QString &sql) {
                QSqlQuery q(db);
                return q.exec(sql) && q.next() ? q.value(0).toInt() : 0;
            };
            result.counts.weatherDays = count("SELECT COUNT(*) FROM weather");
            if (tables.contains("notes"))
                result.counts.noteDays = count("SELECT COUNT(*) FROM notes");
            if (tables.contains("events"))
                result.counts.eventDays = count("SELECT COUNT(DISTINCT date) FROM events");
            return QString();
        }();
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    result.ok = result.problem.isEmpty();
    return result;
}

std::optional<Repository::Counts> Repository::inspect(const QString &file)
{
    const FileCheck check = checkFile(file, false);
    if (!check.ok)
        return std::nullopt;
    return check.counts;
}

bool Repository::replaceWith(const QString &file)
{
    if (m_path == ":memory:" || m_path.isEmpty()) {
        m_error = "This database is not stored in a file.";
        return false;
    }

    // Copy first, then check the copy: the file that is checked is the one that is used,
    // even if the original (on a USB stick, say) changes in between.
    const QString temp = m_path + ".restoring";
    QFile::remove(temp);
    if (!QFile::copy(file, temp)) {
        m_error = "Could not read that file.";
        return false;
    }
    const FileCheck check = checkFile(temp, true);
    if (!check.ok) {
        m_error = check.problem;
        QFile::remove(temp);
        return false;                                   // the current data was never touched
    }

    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    db.close();                                         // Windows cannot replace an open file

    // Move the old file aside instead of deleting it, so a failed swap can be undone.
    const QString aside = m_path + ".replaced";
    QFile::remove(aside);
    bool ok = QFile::rename(m_path, aside);
    if (ok) {
        ok = QFile::rename(temp, m_path);
        if (!ok)
            QFile::rename(aside, m_path);               // put the old data back
    }
    if (ok) {
        QFile::remove(aside);
    } else {
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