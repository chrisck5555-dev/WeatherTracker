#include "ProblemReport.h"

#include <QDateTime>
#include <QFile>
#include <QSysInfo>
#include <QTextStream>

#include "Repository.h"
#include "Version.h"

namespace ProblemReport {

QString build(const Repository &repo, const QString &logTail)
{
    const Repository::Counts c = repo.counts();
    QString r;
    QTextStream t(&r);
    t << "Weather Tracker problem report\n";
    t << "Created: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n\n";
    t << "Program version: " << APP_VERSION << "\n";
    t << "Windows: " << QSysInfo::prettyProductName() << " (" << QSysInfo::currentCpuArchitecture() << ")\n";
    t << "Qt version: " << qVersion() << "\n\n";
    t << "Settings\n";
    t << "  Units: " << repo.setting("units", "metric") << "\n";
    t << "  Text size: " << repo.setting("text_size", "normal") << "\n";
    t << "  Help shown: " << (repo.setting("help_visible", "1") == "1" ? "yes" : "no") << "\n\n";
    t << "Saved data (counts only)\n";
    t << "  Days with weather: " << c.weatherDays << "\n";
    t << "  Days with a note: " << c.noteDays << "\n";
    t << "  Days with events: " << c.eventDays << "\n\n";
    t << "Recent errors\n";
    t << (logTail.isEmpty() ? QString("  (none)") : logTail) << "\n";
    return r;
}

QString write(const Repository &repo, const QString &folder, const QString &logTail)
{
    const QString path = folder + "/problem-report.txt";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QString();
    f.write(build(repo, logTail).toUtf8());
    return path;
}

} // namespace ProblemReport