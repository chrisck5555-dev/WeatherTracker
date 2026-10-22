#pragma once

#include <QString>

class Repository;

namespace ProblemReport {

// The report text. Counts only: never weather values or note text.
QString build(const Repository &repo, const QString &logTail);

// Writes problem-report.txt into `folder`; returns the full path, or "" on failure.
QString write(const Repository &repo, const QString &folder, const QString &logTail);

} // namespace ProblemReport