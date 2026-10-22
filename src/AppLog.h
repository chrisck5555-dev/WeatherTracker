#pragma once

#include <QString>

// A private error log in the data folder. Never throws, never shows UI.
namespace AppLog {

void init(const QString &folder);        // call once at startup; trims an oversized log
QString path();                          // full path of error.log
void write(const QString &message);      // appends "date time version: message"
QString tail(int lines);                 // last N lines, for the problem report

// Installs a Qt message handler so qWarning()/qCritical() also land in the log.
void installQtHandler();

} // namespace AppLog