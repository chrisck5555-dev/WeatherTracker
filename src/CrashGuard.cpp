#include "CrashGuard.h"

#include <QMessageBox>
#include <exception>

#include "AppLog.h"

namespace {
bool g_showing = false;                   // true while a box is already open
}

void reportUnexpectedError(const QString &what)
{
    AppLog::write("Unexpected error: " + what);
    if (g_showing)
        return;                           // log every one, but only ask once
    g_showing = true;
    QMessageBox::warning(nullptr, "Weather Tracker",
        "Something went wrong, but your weather records are safe.\n\n"
        "The program will keep running. If this keeps happening, open Settings and choose "
        "\"Show my problem report\".\n\n"
        "Details were saved to:\n" + AppLog::path());
    g_showing = false;
}

bool GuardedApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception &e) {
        reportUnexpectedError(QString::fromUtf8(e.what()));
    } catch (...) {
        reportUnexpectedError("unknown error");
    }
    return false;
}