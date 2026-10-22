#pragma once

#include <QApplication>

// QApplication that survives exceptions thrown while handling events.
class GuardedApplication : public QApplication
{
public:
    using QApplication::QApplication;
    bool notify(QObject *receiver, QEvent *event) override;
};

// Logs the problem and shows ONE friendly box, however many errors follow.
void reportUnexpectedError(const QString &what);