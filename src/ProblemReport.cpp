#include "ProblemReport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

#include "Repository.h"
#include "Version.h"

namespace ProblemReport {

QString redact(const QString &text, const QString &homePath, const QString &userName)
{
    QString out = text;
    const QString home = QDir::cleanPath(homePath);              // forward slashes
    if (home.size() > 3) {                                       // never "C:/" on its own
        out.replace(home, "<user>", Qt::CaseInsensitive);
        out.replace(QDir::toNativeSeparators(home), "<user>", Qt::CaseInsensitive);
        QString back = home;
        out.replace(back.replace('/', '\\'), "<user>", Qt::CaseInsensitive);
    }
    if (userName.size() >= 3) {
        const QRegularExpression word(
            "\\b" + QRegularExpression::escape(userName) + "\\b",
            QRegularExpression::CaseInsensitiveOption);
        out.replace(word, "<user>");
    }
    return out;
}

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
    t << "  Help shown: " << (repo.setting("help_visible", "1") == "1" ? "yes" : "no") << "\n";
    t << "  Update check: " << (repo.setting("update_check", "on") == "off" ? "off" : "on") << "\n\n";
    t << "Saved data (counts only)\n";
    t << "  Days with weather: " << c.weatherDays << "\n";
    t << "  Days with a note: " << c.noteDays << "\n";
    t << "  Days with events: " << c.eventDays << "\n\n";
    t << "Recent errors\n";
    t << (logTail.isEmpty() ? QString("  (none)") : logTail) << "\n";
    t.flush();

    QString user = qEnvironmentVariable("USERNAME");               // Windows
    if (user.isEmpty())
        user = qEnvironmentVariable("USER");
    return redact(r, QDir::homePath(), user);
}

QString write(const QString &folder, const QString &text)
{
    const QString path = folder + "/problem-report.txt";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QString();
    f.write(text.toUtf8());
    return path;
}

} // namespace ProblemReport
