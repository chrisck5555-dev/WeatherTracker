#include "Display.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>

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

QSize unscaledAvailableSize()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QSize(TextSize::kMinWidth, TextSize::kMinHeight);
    // Under a scale factor Qt reports a smaller logical screen. Multiplying the
    // factor back in gives the size we'd see at 1.0, so the answer doesn't depend
    // on which size we happen to be running at.
    const QSize a = screen->availableGeometry().size();
    const double f = currentScaleFactor();
    return QSize(qRound(a.width() * f), qRound(a.height() * f));
}

QList<TextSize::Size> allowedTextSizes()
{
    const QSize s = unscaledAvailableSize();
    return TextSize::fitting(s.width(), s.height());
}

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