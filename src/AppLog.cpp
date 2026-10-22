#include "AppLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include "Version.h"

namespace AppLog {

namespace {
QString g_path;
QMutex g_lock;                            // messages can arrive from any thread
constexpr qint64 kMaxBytes = 200 * 1024;
QtMessageHandler g_previous = nullptr;

void appendLine(const QString &line)
{
    if (g_path.isEmpty())
        return;
    QFile f(g_path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;                           // a broken log must never break the app
    QTextStream out(&f);
    out << line << '\n';
}

void qtHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg) {       // warnings and worse only
        const char *kind = type == QtWarningMsg ? "warning" : "error";
        AppLog::write(QString("Qt %1: %2").arg(kind, msg));
    }
    if (g_previous)
        g_previous(type, ctx, msg);                      // keep the normal console output
}
} // namespace

void init(const QString &folder)
{
    QMutexLocker lock(&g_lock);
    QDir().mkpath(folder);
    g_path = folder + "/error.log";

    QFile f(g_path);
    if (f.exists() && f.size() > kMaxBytes && f.open(QIODevice::ReadOnly)) {
        f.seek(f.size() - kMaxBytes / 2);                // keep the newest half
        f.readLine();                                    // drop the cut-off first line
        const QByteArray keep = f.readAll();
        f.close();
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(keep);
    }
}

QString path() { return g_path; }

void write(const QString &message)
{
    QMutexLocker lock(&g_lock);
    appendLine(QString("%1  v%2: %3")
                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"),
                        APP_VERSION, message));
}

QString tail(int lines)
{
    QMutexLocker lock(&g_lock);
    QFile f(g_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    const QStringList all = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    return all.mid(qMax(0, int(all.size()) - lines)).join('\n');
}

void installQtHandler()
{
    g_previous = qInstallMessageHandler(qtHandler);
}

} // namespace AppLog