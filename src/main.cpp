#include <QDir>
#include <QMessageBox>
#include <cmath>
#include <exception>

#include "AppLog.h"
#include "CrashGuard.h"
#include "Display.h"
#include "MainWindow.h"
#include "Repository.h"
#include "TextSize.h"
#include "ThemeManager.h"

static int runApp(int argc, char *argv[])
{
    GuardedApplication app(argc, argv);
    QCoreApplication::setApplicationName("WeatherTracker");   // names the AppData folder

    app.setWindowIcon(QIcon(":/icons/app-icon-256.png"));

    // WT_DB lets a developer open a different file (for example demo data).
    const QString override = qEnvironmentVariable("WT_DB");
    const QString dataPath = override.isEmpty() ? Repository::defaultDataPath() : override;
    AppLog::init(QFileInfo(dataPath).absolutePath());
    AppLog::installQtHandler();
    AppLog::write("Program started");

    Repository repo(dataPath);
    if (!repo.isOpen()) {
        AppLog::write("Could not open the data file: " + repo.lastError());
        QMessageBox::critical(nullptr, "Weather Tracker",
                              "Could not open the weather data file.\n\n" + repo.lastError() +
                              "\n\nDetails were saved to:\n" + AppLog::path());
        return 1;
    }

    // Text size: the saved choice, or the largest one that still fits this screen.
    const auto saved = TextSize::fromSetting(repo.setting("text_size", "normal"));
    const auto size = TextSize::effective(saved, Display::allowedTextSizes());
    const double wanted = TextSize::factor(size);
    if (!Display::wasRelaunched() && std::abs(wanted - Display::currentScaleFactor()) > 0.001) {
        if (Display::relaunchWithScale(wanted))
            return 0;
    }

    ThemeManager themes(repo);
    MainWindow window(repo);
    window.show();
    const int code = app.exec();
    AppLog::write("Program closed normally");
    return code;
}

int main(int argc, char *argv[])
{
    // The startup guard: a windowed program has no console, so a failure to launch
    // would otherwise just mean no window and no explanation.
    try {
        return runApp(argc, argv);
    } catch (const std::exception &e) {
        AppLog::write(QString("Startup failed: ") + e.what());
        QMessageBox::critical(nullptr, "Weather Tracker",
            QString("Weather Tracker could not start.\n\n%1\n\nDetails were saved to:\n%2")
                .arg(QString::fromUtf8(e.what()), AppLog::path()));
    } catch (...) {
        AppLog::write("Startup failed: unknown error");
        QMessageBox::critical(nullptr, "Weather Tracker",
            "Weather Tracker could not start.\n\nDetails were saved to:\n" + AppLog::path());
    }
    return 1;
}