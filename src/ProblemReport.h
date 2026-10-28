#pragma once

#include <QString>

class Repository;

namespace ProblemReport {

// The report text. Counts only: never weather values or note text. Her Windows user
// name and home folder are replaced with <user> (see redact).
QString build(const Repository &repo, const QString &logTail);

// Writes `text` as problem-report.txt into `folder`; returns the full path, or "" on failure.
QString write(const QString &folder, const QString &text);

// Replaces the home folder (with either kind of slash, any letter case) and the user name
// with "<user>". A user name shorter than 3 letters is left alone: too likely to be part
// of ordinary words.
QString redact(const QString &text, const QString &homePath, const QString &userName);

} // namespace ProblemReport
