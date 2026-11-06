#include "Display.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>

#ifdef Q_OS_WIN
#ifndef NOMINMAX                                // MinGW often defines it already
#define NOMINMAX                                // keep std::min/std::max usable
#endif
#include <windows.h>
#ifndef SPI_GETCLIENTAREAANIMATION
#define SPI_GETCLIENTAREAANIMATION 0x1042       // Windows Vista and newer
#endif
#endif

namespace Display {

double currentScaleFactor()
{
    bool ok = false;
    const double f = qEnvironmentVariable("QT_SCALE_FACTOR", "1").toDouble(&ok);
    return (ok && f > 0.1) ? f : 1.0;
}

bool wasRelaunched() { return qEnvironmentVariableIsSet("WT_RELAUNCHED"); }

bool animationsAllowed()
{
#ifdef Q_OS_WIN
    BOOL on = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0))
        return on != FALSE;
#endif
    return true;
}

bool relaunchWithScale(double factor)
{
    // A new process inherits our environment, so set the variables first.
    qputenv("QT_SCALE_FACTOR", QByteArray::number(factor, 'f', 2));
    qputenv("WT_RELAUNCHED", "1");
    return QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                   QCoreApplication::arguments().mid(1));
}

} // namespace Display
