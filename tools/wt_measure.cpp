// Stage 4, step 5 of the 1.2.0 plan: measures this PC so the overhaul's numbers (glass cost,
// motion timing) come from a real Windows machine. Not part of the app and never shipped.
//
// Run from the project folder:   .\build-release\wt_measure.exe
//   (add --timing for run 2: only the frame-timing comparison, into measure-timing-<Hz>hz.txt)
// It opens a test window a few times (about two minutes; don't touch the mouse) and writes
// build-release\measure-results.txt. It uses a throwaway data file, never the real one, and
// makes no backups.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QThread>
#include <QVariantAnimation>
#include <QWindow>
#include <chrono>
#include <QtMath>
#include <QAbstractAnimation>
#include <algorithm>
#include <atomic>
#include <functional>
#include <thread>

#include "Display.h"
#include "MainWindow.h"
#include "Repository.h"
#include "ThemeManager.h"
#include "ViewTransition.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <dxgi.h>
#endif

namespace {

bool g_timing = false;   // --timing: only the frame-timing comparison (run 2)
bool g_rhi = false;      // --rhi: this run draws through the graphics card (QT_WIDGETS_RHI=1)

QString resultsPath()
{
    const QString name = qEnvironmentVariable("WT_MEASURE_FILE");   // set by the first process
    return QCoreApplication::applicationDirPath() + "/"
           + (!name.isEmpty() ? name : g_timing ? "measure-timing.txt" : "measure-results.txt");
}

void out(const QString &line)
{
    QFile f(resultsPath());
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << line << "\n";
    QTextStream(stdout) << line << Qt::endl;
}

struct Spread { double median = 0, p95 = 0, worst = 0; };
Spread spread(QList<double> v)
{
    Spread s;
    if (v.isEmpty()) return s;
    std::sort(v.begin(), v.end());
    s.median = v[v.size() / 2];
    s.p95 = v[qMin(v.size() - 1, int(v.size() * 0.95))];
    s.worst = v.last();
    return s;
}

QString ms(double v) { return QString::number(v, 'f', 2) + " ms"; }

// Waits while the app runs normally. (QTest::qWait was used before, but it sleeps in 10 ms
// steps between handling events, which delayed animation frames and spoiled runs 1-4.)
void waitMs(int msecs)
{
    QEventLoop loop;
    QTimer::singleShot(msecs, Qt::PreciseTimer, &loop, &QEventLoop::quit);
    loop.exec();
}

QPushButton *button(QWidget *root, const QString &text)
{
    for (auto *b : root->findChildren<QPushButton *>())
        if (b->text().contains(text) && b->isVisible()) return b;
    return nullptr;
}

// --- Glass (rule G11): the app's own three-pass box blur, the same code as the Linux test ---

void boxRows(QImage &img, int r)
{
    const int w = img.width(), h = img.height();
    QVector<quint32> line(w);
    for (int y = 0; y < h; ++y) {
        auto *p = reinterpret_cast<quint32 *>(img.scanLine(y));
        std::copy(p, p + w, line.begin());
        int s[4] = {0, 0, 0, 0};
        auto px = [&](int x) { return line[qBound(0, x, w - 1)]; };
        for (int x = -r; x <= r; ++x) { const quint32 c = px(x); for (int k = 0; k < 4; ++k) s[k] += (c >> (8 * k)) & 0xff; }
        const int n = 2 * r + 1;
        for (int x = 0; x < w; ++x) {
            quint32 o = 0; for (int k = 0; k < 4; ++k) o |= quint32(s[k] / n) << (8 * k);
            p[x] = o;
            const quint32 a = px(x + r + 1), b = px(x - r);
            for (int k = 0; k < 4; ++k) s[k] += int((a >> (8 * k)) & 0xff) - int((b >> (8 * k)) & 0xff);
        }
    }
}

void boxBlur(QImage &img, int r)
{
    if (r < 1) return;
    for (int i = 0; i < 3; ++i) boxRows(img, r);
    QImage t = img.transformed(QTransform().rotate(90));
    for (int i = 0; i < 3; ++i) boxRows(t, r);
    img = t.transformed(QTransform().rotate(-90));
}

// Paints a still picture of the window, moving 24 px and fading in: the size of motion the
// rules allow (M13). Records when each frame is painted.
class PacingProbe : public QWidget
{
public:
    explicit PacingProbe(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    QPixmap picture;
    double offset = 0, opacity = 1;
    bool immediate = false;         // draw at once on each animation step instead of on the next update
    QElapsedTimer *clock = nullptr;
    QList<double> stamps;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (clock) stamps << clock->nsecsElapsed() / 1e6;
        QPainter p(this);
        p.fillRect(rect(), palette().window());
        p.setOpacity(opacity);
        p.drawPixmap(QPointF(qRound(offset), 0), picture);
    }
};

// fps: frames per second. uneven: gaps over 1.5x the usual gap (a visible hitch).
// skipped: gaps over 1.5 screen refreshes (a refresh went by with no new frame).
void writeGaps(const QString &name, const QList<double> &gaps, const QList<double> &firsts, double frameMs)
{
    const Spread g = spread(gaps), f = spread(firsts);
    int uneven = 0, skipped = 0;
    double sum = 0;
    for (double x : gaps) {
        sum += x;
        if (x > g.median * 1.5) ++uneven;
        if (x > frameMs * 1.5) ++skipped;
    }
    const auto pct = [&](int n) { return gaps.isEmpty() ? 0 : qRound(100.0 * n / gaps.size()); };
    out(QString("  %1: %2 frames/s | gap median %3, 95th %4, worst %5 | uneven %6% | skipped a refresh %7% | first frame %8")
            .arg(name, -24).arg(sum > 0 ? qRound(gaps.size() * 1000.0 / sum) : 0)
            .arg(ms(g.median), ms(g.p95), ms(g.worst))
            .arg(pct(uneven)).arg(pct(skipped)).arg(ms(f.median)));
}

#ifdef _WIN32
// The display's vertical blank: DXGI's WaitForVBlank returns once per screen refresh.
IDXGIOutput *vblankOutput()
{
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))))
        return nullptr;
    IDXGIAdapter1 *adapter = nullptr;
    IDXGIOutput *output = nullptr;
    if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
        if (FAILED(adapter->EnumOutputs(0, &output))) output = nullptr;
        adapter->Release();
    }
    factory->Release();
    return output;
}
#endif

// Fix 2: advance animations once per screen refresh. A helper thread waits for the screen's
// vertical blank (DXGI WaitForVBlank, or DwmFlush if that isn't available) and asks the main
// thread for a frame. (On other systems, for testing only, it waits 1/60 s instead.)
// The driver's own elapsed() is used: time since the driver started, as Qt expects.
class RefreshDriver : public QAnimationDriver
{
public:
    ~RefreshDriver() override { stopThread(); }

protected:
    void start() override
    {
        QAnimationDriver::start();
        if (m_thread.joinable()) return;
        m_run = true;
        m_thread = std::thread([this] {
            QElapsedTimer loop;
            loop.start();
#ifdef _WIN32
            IDXGIOutput *output = vblankOutput();
            m_source = output ? 1 : 2;
#endif
            while (m_run) {
#ifdef _WIN32
                // Both normally wait for the next refresh; never spin if they don't.
                const bool ok = output ? SUCCEEDED(output->WaitForVBlank()) : SUCCEEDED(DwmFlush());
                if (!ok || loop.restart() < 2) Sleep(1);
#else
                std::this_thread::sleep_for(std::chrono::microseconds(16667));
#endif
                if (!m_pending.exchange(true))
                    QMetaObject::invokeMethod(this, [this] { m_pending = false; if (isRunning()) advance(); },
                                              Qt::QueuedConnection);
            }
#ifdef _WIN32
            if (output) output->Release();
#endif
        });
    }
    void stop() override { stopThread(); QAnimationDriver::stop(); }

public:
    QString source() const
    {
        return m_source == 1 ? "DXGI WaitForVBlank" : m_source == 2 ? "DwmFlush" : "a 1/60 s wait (test only)";
    }

private:
    void stopThread() { m_run = false; if (m_thread.joinable()) m_thread.join(); }
    std::thread m_thread;
    std::atomic<bool> m_run{false}, m_pending{false};
    std::atomic<int> m_source{0};
};

// Fix 3: advance animations when Qt says the window is ready for its next frame
// (QWindow::requestUpdate, Qt's own frame scheduling), then paint in that same frame.
class WindowFrameDriver : public QAnimationDriver
{
public:
    explicit WindowFrameDriver(QWindow *window) : m_window(window) { window->installEventFilter(this); }
    ~WindowFrameDriver() override { m_window->removeEventFilter(this); }

protected:
    void start() override { QAnimationDriver::start(); m_window->requestUpdate(); }
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_window && event->type() == QEvent::UpdateRequest && isRunning()) {
            advance();
            if (isRunning()) m_window->requestUpdate();
        }
        return false;
    }

private:
    QWindow *m_window;
};

// ------------------------------------------------------------------------------------------
// One measured run, in a child process started with a given QT_SCALE_FACTOR.
int runOne(const QString &label, int w, int h, QApplication &app)
{
    QTemporaryDir home;
    qputenv("WT_DB", home.filePath("measure.db").toUtf8());   // also turns automatic backups off
    Repository repo(home.filePath("measure.db"));
    if (!repo.isOpen()) { out("Could not open the test data file."); return 1; }
    repo.setSetting("tour_done", "1");
    repo.setSetting("help_visible", "0");
    repo.setSetting("style", "clay");
    repo.setSetting("brightness", "light");
    const QDate today = QDate::currentDate();
    for (int i = 0; i < 400; ++i) {
        WeatherDay d; d.highC = 5 + (i * 37 % 17); d.lowC = -3 + (i * 13 % 9); d.cloud = "Overcast";
        d.precipType = "Rain"; d.precipMm = i % 5;
        repo.saveWeather(today.addDays(-i), d);
        if (i % 3 == 0) repo.saveNote(today.addDays(-i), "note");
    }
    ThemeManager themes(repo);
    MainWindow window(repo);
    window.resize(w, h);
    window.show();
    window.raise();
    window.activateWindow();
    waitMs(600);

    const qreal dpr = window.devicePixelRatioF();
    QScreen *screen = window.screen();
    const double hz = screen ? screen->refreshRate() : 60.0;
    const double frameMs = 1000.0 / (hz > 1 ? hz : 60.0);
    out("");
    out(QString("=== %1: window %2 x %3, scaling %4%, screen %5 Hz%6 ===")
            .arg(label).arg(window.width()).arg(window.height())
            .arg(qRound(dpr * 100)).arg(hz, 0, 'f', 0)
            .arg(g_rhi ? ", drawn through the graphics card" : ""));
    if (g_rhi) {
        const bool direct3d = window.windowHandle() && window.windowHandle()->surfaceType() == QSurface::Direct3DSurface;
        out(direct3d ? "(confirmed: the window is presented with Direct3D 11, in step with the screen)"
                     : "(the graphics-card path did NOT turn on; these numbers are the normal path)");
    }

    auto *tabs = window.findChild<QTabWidget *>();
    if (!tabs) { out("No tabs found."); return 1; }

    if (!g_timing) {
        // 1. Drawing cost of each screen (the whole window, no animation).
        out("Drawing the whole window (median of 10):");
        auto paintCost = [&](const QString &name) {
            QList<double> v;
            for (int i = 0; i < 10; ++i) { QElapsedTimer t; t.start(); window.repaint(); v << t.nsecsElapsed() / 1e6; }
            out(QString("  %1 %2").arg(name, -12).arg(ms(spread(v).median)));
        };
        tabs->setCurrentIndex(0);
        for (const char *period : {"Day", "Week", "Month", "Year"}) {
            if (auto *b = button(&window, period)) { QTest::mouseClick(b, Qt::LeftButton); waitMs(250); paintCost(period); }
        }
        if (auto *b = button(&window, "Month")) { QTest::mouseClick(b, Qt::LeftButton); waitMs(250); }
        for (int t = 1; t < tabs->count(); ++t) {
            tabs->setCurrentIndex(t); waitMs(250); paintCost(tabs->tabText(t));
        }

        // 2. Glass (G11), both ways, for the strip under the bars.
        const int stripH = 164;   // L9: the bars' height at Normal text
        out("Glass, way 1: draw and blur the strip under the bars on every frame (median of 15):");
        for (int t = 0; t < tabs->count(); ++t) {
            tabs->setCurrentIndex(t); waitMs(250);
            QWidget *page = tabs->currentWidget();
            const int pw = page->width();
            QImage target(QSize(pw, stripH) * dpr, QImage::Format_ARGB32_Premultiplied);
            QList<double> total;
            for (int i = 0; i < 15; ++i) {
                QElapsedTimer c; c.start();
                QImage quarter(qCeil(pw * dpr / 4), qCeil(stripH * dpr / 4), QImage::Format_ARGB32_Premultiplied);
                quarter.fill(Qt::transparent);
                { QPainter p(&quarter); p.scale(dpr / 4, dpr / 4); page->render(&p, QPoint(), QRegion(0, 0, pw, stripH), QWidget::DrawChildren); }
                boxBlur(quarter, qRound(24 * dpr / 4 / 2));
                { QPainter p(&target); p.setRenderHint(QPainter::SmoothPixmapTransform);
                  p.drawImage(target.rect(), quarter); p.fillRect(target.rect(), QColor(246, 248, 253, 217)); }
                total << c.nsecsElapsed() / 1e6;
            }
            out(QString("  %1 %2 per frame").arg(tabs->tabText(t), -12).arg(ms(spread(total).median)));
        }
        out("Glass, way 2: blur the whole page once, then scrolling only moves the copy (median of 8 and of 160):");
        for (int t = 0; t < tabs->count(); ++t) {
            tabs->setCurrentIndex(t); waitMs(250);
            QWidget *page = tabs->currentWidget();
            QWidget *content = page;
            if (auto *sa = page->findChild<QScrollArea *>(); sa && sa->widget()) content = sa->widget();
            const QSize cs = content->size();
            QImage target(QSize(page->width(), stripH) * dpr, QImage::Format_ARGB32_Premultiplied);
            QList<double> once, frame;
            for (int i = 0; i < 8; ++i) {
                QElapsedTimer c; c.start();
                QImage quarter(qCeil(cs.width() * dpr / 4), qCeil(cs.height() * dpr / 4), QImage::Format_ARGB32_Premultiplied);
                quarter.fill(Qt::transparent);
                { QPainter p(&quarter); p.scale(dpr / 4, dpr / 4); content->render(&p, QPoint(), QRegion(), QWidget::DrawChildren); }
                boxBlur(quarter, qRound(24 * dpr / 4 / 2));
                once << c.nsecsElapsed() / 1e6;
                for (int y = 0; y < 20; ++y) {
                    c.start();
                    const QRect src(0, qMin(quarter.height() - 1, int(y * 40 * dpr / 4)), quarter.width(), qCeil(stripH * dpr / 4));
                    { QPainter p(&target); p.setRenderHint(QPainter::SmoothPixmapTransform);
                      p.drawImage(target.rect(), quarter, src); p.fillRect(target.rect(), QColor(246, 248, 253, 217)); }
                    frame << c.nsecsElapsed() / 1e6;
                }
            }
            out(QString("  %1 once %2 | each scroll frame %3").arg(tabs->tabText(t), -12)
                    .arg(ms(spread(once).median), ms(spread(frame).median)));
        }
    }
    tabs->setCurrentIndex(0); waitMs(250);

    // 3. Frame pacing of a quarter, still-picture motion (24 px, fade in), as the rules allow.
    out(QString("Motion frame pacing (one screen refresh at %1 Hz = %2):").arg(hz, 0, 'f', 0).arg(ms(frameMs)));
    {
        PacingProbe probe(window.centralWidget());
        probe.setGeometry(window.centralWidget()->rect());
        probe.picture = window.centralWidget()->grab();
        QElapsedTimer clock; clock.start();
        probe.clock = &clock;
        probe.show(); probe.raise();
        waitMs(200);
        auto runs = [&](int duration, int count) {
            QList<double> gaps, firsts;
            for (int r = 0; r < count; ++r) {
                probe.stamps.clear();
                QVariantAnimation anim;
                anim.setStartValue(0.0); anim.setEndValue(1.0); anim.setDuration(duration);
                anim.setEasingCurve(QEasingCurve::OutCubic);
                QObject::connect(&anim, &QVariantAnimation::valueChanged, &probe, [&](const QVariant &v) {
                    const double x = v.toDouble();
                    probe.offset = 24 * (1 - x); probe.opacity = 0.3 + 0.7 * x;
                    if (probe.immediate) probe.repaint(); else probe.update();
                });
                const double t0 = clock.nsecsElapsed() / 1e6;
                anim.start();
                waitMs(duration + 150);
                if (!probe.stamps.isEmpty()) firsts << probe.stamps.first() - t0;
                for (int i = 1; i < probe.stamps.size(); ++i) gaps << probe.stamps[i] - probe.stamps[i - 1];
            }
            return std::make_pair(gaps, firsts);
        };
        auto measure = [&](const QString &how) {
            out("  " + how + ":");
            auto [g200, f200] = runs(200, 12);
            writeGaps("  200 ms move, 12 times", g200, f200, frameMs);
            auto [g1000, f1000] = runs(1000, 2);
            writeGaps("  1 s move, 2 times", g1000, f1000, frameMs);
        };
        measure(g_rhi ? "Fix 4: graphics card + Qt's default clock" : "Qt's default animation clock");
        if (g_timing) {
            // (Fix 1, a 1 ms Windows timer, made no difference in run 2 and was removed.)
            for (bool immediate : {true}) {           // fix 2 without 2b was no better and was dropped
                RefreshDriver driver;                 // fix 2 and 2b
                driver.install();
                probe.immediate = immediate;
                measure(QString(g_rhi ? "Fix 4 + " : "")
                        + (immediate ? "Fix 2b: timed to the screen refresh, drawn at once"
                                     : "Fix 2: frames timed to the screen refresh"));
                probe.immediate = false;
                driver.uninstall();
                out("    (refresh signal: " + driver.source() + ")");
                waitMs(100);
            }
            {
                WindowFrameDriver driver(window.windowHandle());   // fix 3
                driver.install();
                measure(QString(g_rhi ? "Fix 4 + " : "") + "Fix 3: frames when Qt says the window is ready");
                driver.uninstall();
            }
        }
        probe.hide();
    }

    // 4. The 1.1.0 slide (turned off in 1.1.0), for comparison with the Linux numbers.
    if (g_timing) { window.close(); app.processEvents(); return 0; }
    out("The 1.1.0 page slide, Next month 6 times (for comparison):");
    if (auto *transition = window.findChild<ViewTransition *>()) {
        transition->setEnabled(true);
        QWidget *cover = window.findChild<QWidget *>("viewTransitionCover");
        QPushButton *next = button(&window, "Next");
        if (cover && next) {
            class Log : public QObject {
            public:
                QElapsedTimer *clock; QList<double> stamps;
                bool eventFilter(QObject *, QEvent *e) override
                { if (e->type() == QEvent::Paint) stamps << clock->nsecsElapsed() / 1e6; return false; }
            } log;
            QElapsedTimer clock; clock.start(); log.clock = &clock;
            cover->installEventFilter(&log);
            QList<double> gaps, firsts, clicks;
            for (int r = 0; r < 6; ++r) {
                log.stamps.clear();
                const double t0 = clock.nsecsElapsed() / 1e6;
                QTest::mouseClick(next, Qt::LeftButton);
                clicks << clock.nsecsElapsed() / 1e6 - t0;
                waitMs(ViewTransition::kDurationMs + 250);
                if (!log.stamps.isEmpty()) firsts << log.stamps.first() - t0;
                for (int i = 1; i < log.stamps.size(); ++i) gaps << log.stamps[i] - log.stamps[i - 1];
            }
            cover->removeEventFilter(&log);
            writeGaps("old slide", gaps, firsts, frameMs);
            out(QString("  time inside the click (taking the 'before' picture): median %1").arg(ms(spread(clicks).median)));
        } else {
            out("  (not found)");
        }
        transition->setEnabled(false);
    }
    window.close();
    app.processEvents();
    return 0;
}

QString cpuName()
{
#ifdef Q_OS_WIN
    QSettings cpu("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", QSettings::NativeFormat);
    return cpu.value("ProcessorNameString").toString().trimmed();
#else
    return QSysInfo::currentCpuArchitecture();
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);            // never touch the real app's folders
    for (int i = 1; i < argc; ++i)
        if (QString(argv[i]) == "--timing") g_timing = true;
        else if (QString(argv[i]) == "--rhi") g_rhi = true;
    const bool child = argc >= 5 && QString(argv[1]) == "--run";
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("WeatherTrackerMeasure");

    if (child)
        return runOne(QString::fromLocal8Bit(argv[2]), QString(argv[3]).toInt(), QString(argv[4]).toInt(), app);

    // Parent: describe the PC, then measure at 100% and 150% scaling in separate runs.
    QScreen *screen = QGuiApplication::primaryScreen();
    if (g_timing && screen)    // one file per refresh rate, so a 60 Hz run doesn't replace a 120 Hz one
        qputenv("WT_MEASURE_FILE", QString("measure-timing-%1hz.txt").arg(qRound(screen->refreshRate())).toUtf8());
    QFile::remove(resultsPath());
    const double native = screen ? screen->devicePixelRatio() : 1.0;
    out(g_timing ? "Weather Tracker frame-timing test for the 1.2.0 plan (Stage 4, step 5, run 5: fixed waiting)"
                 : "Weather Tracker measurements for the 1.2.0 plan (Stage 4, step 5)");
    out("Date: " + QDateTime::currentDateTime().toString(Qt::ISODate));
    out("Windows: " + QSysInfo::prettyProductName() + " (" + QSysInfo::kernelVersion() + ")");
    out("Processor: " + cpuName() + ", " + QString::number(QThread::idealThreadCount()) + " threads");
    out("Qt: " + QString(qVersion()));
    if (screen)
        out(QString("Screen: %1 x %2 (logical), Windows scaling %3%, %4 Hz")
                .arg(screen->size().width()).arg(screen->size().height())
                .arg(qRound(native * 100)).arg(screen->refreshRate(), 0, 'f', 0));
    out(QString("Windows animation effects: %1").arg(Display::animationsAllowed() ? "on" : "off"));
#ifdef Q_OS_WIN
    QSettings personalize("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    out(QString("Windows transparency effects: %1").arg(personalize.value("EnableTransparency", 1).toInt() ? "on" : "off"));
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power))
        out(QString("Power: %1, battery saver %2")
                .arg(power.ACLineStatus == 1 ? "plugged in" : power.ACLineStatus == 0 ? "on battery" : "unknown")
                .arg(power.SystemStatusFlag == 1 ? "on" : "off"));
#endif

    struct Config { const char *label; double scale; int w, h; };
    const Config configs[] = {
        {"100% (like 1366 x 768)", 1.0, 1366, 768},
        {"150% (like 1920 x 1080)", 1.5, 1280, 720},
    };
    for (const auto &c : configs) {
        // In the timing test, each size runs twice: the normal way, then with Fix 4 (Qt draws
        // the window through the graphics card, Direct3D 11, which waits for the screen).
        for (bool rhi : g_timing ? QList<bool>{false, true} : QList<bool>{false}) {
            QProcess p;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert("QT_SCALE_FACTOR", QString::number(c.scale / native, 'f', 4));
            if (rhi) env.insert("QT_WIDGETS_RHI", "1");
            p.setProcessEnvironment(env);
            p.setProcessChannelMode(QProcess::ForwardedChannels);
            p.start(QCoreApplication::applicationFilePath(),
                    QStringList{"--run", c.label, QString::number(c.w), QString::number(c.h)}
                        + (g_timing ? QStringList{"--timing"} : QStringList{})
                        + (rhi ? QStringList{"--rhi"} : QStringList{}));
            if (!p.waitForFinished(5 * 60 * 1000))
                out(QString("The %1 run did not finish.").arg(c.label));
            else if (p.exitStatus() == QProcess::CrashExit || p.exitCode() != 0)
                out(QString("The %1 run%2 stopped with an error (code %3).")
                        .arg(c.label, rhi ? " through the graphics card" : "").arg(p.exitCode()));
        }
    }
    out("");
    out("Done. Results are in " + QDir::toNativeSeparators(resultsPath()));
    return 0;
}
