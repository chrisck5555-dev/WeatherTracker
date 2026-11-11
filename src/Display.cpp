#include "Display.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QSettings>

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

bool transparencyOff()
{
#ifdef Q_OS_WIN
    const QSettings personalize(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        QSettings::NativeFormat);
    return personalize.value("EnableTransparency", 1).toInt() == 0;
#else
    return false;
#endif
}

bool batterySaverOn()
{
#ifdef Q_OS_WIN
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status))
        return false;
    // The fourth byte is SystemStatusFlag (1 = battery saver on, Windows 10 and newer).
    // Older MinGW headers call it Reserved1, so it is read by its place.
    return reinterpret_cast<const BYTE *>(&status)[3] == 1;
#else
    return false;
#endif
}

int scrollBarWidth()
{
    static const int width = [] {
#ifdef Q_OS_WIN
        // GetSystemMetrics answers at the system's DPI, so it is turned back into 100% px.
        const int px = GetSystemMetrics(SM_CXVSCROLL);
        int dpi = 96;
        if (HDC screen = GetDC(nullptr)) {
            dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(nullptr, screen);
        }
        if (px > 0 && dpi > 0)
            return std::clamp(int(std::lround(px * 96.0 / dpi)), 12, 64);
#endif
        return 17;
    }();
    return width;
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
