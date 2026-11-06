// The layout check (design-rules RZ8; Stage 4 step 10). Part of wt_measure, never shipped.
//
//   .\build-release\wt_measure.exe --layout
//
// Opens every screen of the program (with a throwaway data file) and checks its widgets
// against the design rules:
//   - at Normal text in the soft light look, at every window width from 512 to 2560 in
//     8 px steps, at three heights
//   - at named window shapes (Snap halves and quarters, common screens, portrait, ultrawide)
//     in every text size and all four looks
// It writes build-release\layout-check.txt and screenshots in build-release\layout-check\.
//
// What it can see: real widgets (buttons, labels, text boxes, tabs, scroll areas, layouts).
// Things the program paints itself (calendar days, charts) are checked on the screenshots.

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontInfo>
#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <algorithm>
#include <functional>
#include <tuple>

#include "Design.h"
#include "MainWindow.h"
#include "Repository.h"
#include "TextSize.h"
#include "ThemeManager.h"
#include "Typography.h"
#include "WeatherDay.h"

namespace {

QString reportPath() { return QCoreApplication::applicationDirPath() + "/layout-check.txt"; }
QString shotDir() { return QCoreApplication::applicationDirPath() + "/layout-check"; }

void report(const QString &line)
{
    QFile f(reportPath());
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << line << "\n";
    QTextStream(stdout) << line << Qt::endl;
}

void settle()
{
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

// A short name for a widget in the report: its kind and its text.
QString describe(const QWidget *w)
{
    QString text;
    if (const auto *b = qobject_cast<const QAbstractButton *>(w))
        text = b->text();
    else if (const auto *l = qobject_cast<const QLabel *>(w))
        text = l->text();
    else if (const auto *e = qobject_cast<const QLineEdit *>(w))
        text = e->accessibleName().isEmpty() ? e->placeholderText() : e->accessibleName();
    if (text.isEmpty())
        text = w->accessibleName();
    text = text.simplified();
    if (text.size() > 40)
        text = text.left(37) + "...";
    QString kind = QString::fromLatin1(w->metaObject()->className());
    if (kind.startsWith('Q'))
        kind = kind.mid(1);
    return text.isEmpty() ? kind : QString("%1 \"%2\"").arg(kind, text);
}

bool shownIn(const QWidget *w, const QWidget *window)
{
    return w->isVisibleTo(window) && w->width() > 0 && w->height() > 0;
}

// --- One finding: which screen, which rule, what -----------------------------------------
struct Key {
    QString screen, rule, what;
    bool operator<(const Key &o) const
    {
        return std::tie(screen, rule, what) < std::tie(o.screen, o.rule, o.what);
    }
};

class Findings
{
public:
    void add(const QString &screen, const QString &rule, const QString &what, const QString &where)
    {
        m_items[{screen, rule, what}].insert(where);
    }
    const QMap<Key, QSet<QString>> &items() const { return m_items; }
    void clear() { m_items.clear(); }

private:
    QMap<Key, QSet<QString>> m_items;
};

// Widths are reported as ranges: "512-1000, 1200".
QString widthRanges(QList<int> widths)
{
    std::sort(widths.begin(), widths.end());
    QStringList parts;
    for (int i = 0; i < widths.size();) {
        int j = i;
        while (j + 1 < widths.size() && widths[j + 1] - widths[j] <= 8)
            ++j;
        parts << (i == j ? QString::number(widths[i])
                         : QString("%1-%2").arg(widths[i]).arg(widths[j]));
        i = j + 1;
    }
    return parts.join(", ");
}

// --- The checks --------------------------------------------------------------------------
void checkWindow(QWidget *window, const QString &screen, const QString &where, Findings &f)
{
    const auto widgets = window->findChildren<QWidget *>();
    QSet<int> textSizes;

    for (QWidget *w : widgets) {
        if (w->isWindow() || !shownIn(w, window))
            continue;
        // Qt's own small parts inside tab bars and scroll bars are not checked.
        if (qobject_cast<QTabBar *>(w->parentWidget()) || qobject_cast<QScrollBar *>(w))
            continue;

        // Z1: everything clickable is at least 44 x 44.
        const bool clickable = (qobject_cast<QAbstractButton *>(w) || qobject_cast<QLineEdit *>(w)
                                || qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w))
                               && w->isEnabled();
        if (clickable && (w->width() < Design::kTarget || w->height() < Design::kTarget))
            f.add(screen, "Z1 target under 44 x 44",
                  QString("%1 is %2 x %3").arg(describe(w)).arg(w->width()).arg(w->height()), where);
        if (auto *bar = qobject_cast<QTabBar *>(w))
            for (int i = 0; i < bar->count(); ++i)
                if (bar->tabRect(i).height() < Design::kTarget)
                    f.add(screen, "Z1 target under 44 x 44",
                          QString("tab \"%1\" is %2 high").arg(bar->tabText(i)).arg(bar->tabRect(i).height()), where);

        // L4: text never cut off.
        if (auto *l = qobject_cast<QLabel *>(w)) {
            if (!l->text().isEmpty() && l->pixmap().isNull()) {
                if (!l->wordWrap() && l->sizeHint().width() > l->width() + 1)
                    f.add(screen, "L4 text cut off", describe(l) + " is too narrow", where);
                if (l->wordWrap() && l->heightForWidth(l->width()) > l->height() + 1)
                    f.add(screen, "L4 text cut off", describe(l) + " is too short", where);
            }
        }
        if (auto *b = qobject_cast<QAbstractButton *>(w))
            if (!b->text().isEmpty() && b->sizeHint().width() > b->width() + 1)
                f.add(screen, "L4 text cut off", describe(b) + " is too narrow", where);

        // RZ3: nothing sticks out of the area it belongs to. (A scroll area's page may be
        // larger than its window onto it: that is what scrolling is for.)
        if (QWidget *parent = w->parentWidget()) {
            const bool scrollPage = parent->parentWidget()
                && qobject_cast<QAbstractScrollArea *>(parent->parentWidget())
                && static_cast<QAbstractScrollArea *>(parent->parentWidget())->viewport() == parent;
            if (!scrollPage && !QRect(QPoint(0, 0), parent->size()).adjusted(-1, -1, 1, 1).contains(w->geometry()))
                f.add(screen, "RZ3 sticks out of its area",
                      QString("%1 inside %2").arg(describe(w), describe(parent)), where);
        }

        // SC1: scrolling is vertical only.
        if (auto *area = qobject_cast<QAbstractScrollArea *>(w))
            if (area->horizontalScrollBar() && area->horizontalScrollBar()->maximum() > 0)
                f.add(screen, "SC1 scrolls sideways", describe(area), where);

        // T6: at most three type sizes on a screen (widgets only).
        const bool hasText = (qobject_cast<QLabel *>(w) && !static_cast<QLabel *>(w)->text().isEmpty())
            || (qobject_cast<QAbstractButton *>(w) && !static_cast<QAbstractButton *>(w)->text().isEmpty())
            || qobject_cast<QLineEdit *>(w) || qobject_cast<QTabBar *>(w);
        if (hasText)
            textSizes.insert(QFontInfo(w->font()).pixelSize());
    }

    // Overlaps between neighbours (widgets with the same parent).
    for (QWidget *parent : widgets + QList<QWidget *>{window}) {
        if (!shownIn(parent, window) && parent != window)
            continue;
        if (qobject_cast<QStackedWidget *>(parent) || qobject_cast<QAbstractScrollArea *>(parent))
            continue;
        QList<QWidget *> kids;
        for (QObject *o : parent->children())
            if (auto *k = qobject_cast<QWidget *>(o); k && !k->isWindow() && shownIn(k, window))
                kids << k;
        for (int i = 0; i < kids.size(); ++i)
            for (int j = i + 1; j < kids.size(); ++j) {
                const QRect both = kids[i]->geometry() & kids[j]->geometry();
                if (both.width() > 1 && both.height() > 1)
                    f.add(screen, "RZ3 overlap",
                          QString("%1 and %2").arg(describe(kids[i]), describe(kids[j])), where);
            }
    }

    // S1: gaps between neighbours in a row or column, and layout margins, on the scale.
    for (QWidget *w : widgets + QList<QWidget *>{window}) {
        if (w != window && !shownIn(w, window))
            continue;
        auto *box = qobject_cast<QBoxLayout *>(w->layout());
        if (!box)
            continue;
        const QMargins m = box->contentsMargins();
        for (int v : {m.left(), m.top(), m.right(), m.bottom()})
            if (!Design::onSpacingScale(v)) {
                f.add(screen, "S1 spacing off the scale",
                      QString("margin of %1 px inside %2").arg(v).arg(describe(w)), where);
                break;
            }
        const bool across = box->direction() == QBoxLayout::LeftToRight
                         || box->direction() == QBoxLayout::RightToLeft;
        QList<QWidget *> items;
        for (int i = 0; i < box->count(); ++i)
            if (QWidget *k = box->itemAt(i)->widget(); k && shownIn(k, window))
                items << k;
        std::sort(items.begin(), items.end(), [across](QWidget *a, QWidget *b) {
            return across ? a->x() < b->x() : a->y() < b->y();
        });
        for (int i = 0; i + 1 < items.size(); ++i) {
            const int gap = across ? items[i + 1]->x() - (items[i]->x() + items[i]->width())
                                   : items[i + 1]->y() - (items[i]->y() + items[i]->height());
            if (gap > 0 && !Design::onSpacingScale(gap))
                f.add(screen, "S1 spacing off the scale",
                      QString("%1 px between %2 and %3").arg(gap).arg(describe(items[i]), describe(items[i + 1])),
                      where);
        }
    }

    if (textSizes.size() > Design::kMaxTextSizesPerScreen) {
        QList<int> sizes(textSizes.begin(), textSizes.end());
        std::sort(sizes.begin(), sizes.end());
        QStringList s;
        for (int v : sizes) s << QString::number(v);
        f.add(screen, "T6 more than 3 text sizes", QString("%1 px").arg(s.join(", ")), where);
    }
}

// --- The screens -------------------------------------------------------------------------
QPushButton *findButton(QWidget *root, const QString &text)
{
    for (auto *b : root->findChildren<QPushButton *>())
        if (b->isVisibleTo(root) && b->text().contains(text))
            return b;
    return nullptr;
}

struct Screen {
    QString name;
    std::function<bool(QWidget *)> open;    // false if it couldn't be opened
    std::function<void(QWidget *)> close;
};

QList<Screen> screens()
{
    QList<Screen> list;
    const QStringList tabNames{"Calendar", "Analytics", "Charts", "Records"};
    const QList<QStringList> periods{{"Day", "Week", "Month", "Year"},
                                     {"Day", "Month", "Season", "Year"},
                                     {"Month", "Season", "Year"},
                                     {"Month", "Season", "Year", "All"}};
    for (int t = 0; t < tabNames.size(); ++t)
        for (const QString &p : periods[t])
            list << Screen{tabNames[t] + ": " + p,
                           [t, p](QWidget *w) {
                               auto *tabs = w->findChild<QTabWidget *>();
                               if (!tabs) return false;
                               tabs->setCurrentIndex(t);
                               settle();
                               for (auto *b : tabs->currentWidget()->findChildren<QPushButton *>())
                                   if (b->isCheckable() && b->text().startsWith(p) && b->isVisibleTo(w)) {
                                       b->click();
                                       settle();
                                       return true;
                                   }
                               return false;
                           },
                           [](QWidget *) {}};
    list << Screen{"Editor",
                   [](QWidget *w) {
                       auto *tabs = w->findChild<QTabWidget *>();
                       if (tabs) tabs->setCurrentIndex(0);
                       settle();
                       QPushButton *b = findButton(w, "today's weather");
                       if (!b) return false;
                       b->click();
                       settle();
                       return true;
                   },
                   [](QWidget *w) {
                       if (QPushButton *b = findButton(w, "Cancel")) b->click();
                       settle();
                   }};
    list << Screen{"Settings",
                   [](QWidget *w) {
                       QPushButton *b = findButton(w, "Settings");
                       if (!b) return false;
                       b->click();
                       settle();
                       return true;
                   },
                   [](QWidget *w) {
                       if (QPushButton *b = findButton(w, "Done")) b->click();
                       settle();
                   }};
    return list;
}

struct Shape { QString name; int w, h; };   // effective pixels, inside the window

QList<Shape> namedShapes()
{
    return {
        {"maximized on 1366 x 768", 1366, 705},
        {"Snap half of 1366 x 768", 683, 705},
        {"Snap quarter of 1366 x 768", 683, 352},
        {"1024 x 640", 1024, 640},
        {"maximized on 1920 x 1080 at 125%", 1536, 809},
        {"maximized on 1920 x 1080 at 150%", 1280, 658},
        {"Snap half of 1920 x 1080 at 150%", 640, 658},
        {"maximized on 1920 x 1080", 1920, 1017},
        {"Snap third of 1920 x 1080", 640, 1017},
        {"portrait 1080 x 1920", 1080, 1857},
        {"ultrawide 3440 x 1440", 3440, 1377},
    };
}

void seed(Repository &repo)
{
    repo.setSetting("tour_done", "1");
    repo.setSetting("help_visible", "0");
    const QDate today = QDate::currentDate();
    for (int i = 1; i < 500; ++i) {
        WeatherDay d;
        d.highC = 5 + (i * 37 % 17);
        d.lowC = -3 + (i * 13 % 9);
        d.cloud = i % 2 ? "Overcast" : "Partly cloudy";
        d.precipType = i % 3 ? "Rain" : "Snow";
        d.precipMm = i % 7;
        d.windKmh = 5 + i % 30;
        d.windDir = "NW";
        repo.saveWeather(today.addDays(-i), d);
        if (i % 4 == 0)
            repo.saveNote(today.addDays(-i), "A note about the day");
    }
}

int runChild(const QString &sizeName)
{
    const TextSize::Size size = TextSize::fromSetting(sizeName);
    const double factor = TextSize::factor(size);
    QTemporaryDir home;
    qputenv("WT_DB", home.filePath("layout.db").toUtf8());   // also turns automatic backups off
    Repository repo(home.filePath("layout.db"));
    if (!repo.isOpen()) {
        report("Could not open the test data file.");
        return 1;
    }
    seed(repo);
    repo.setSetting("style", "clay");
    repo.setSetting("brightness", "light");
    QApplication::setFont(appFont(QApplication::font()));   // the app's own font, as in main.cpp
    ThemeManager themes(repo);
    MainWindow window(repo);
    window.setAttribute(Qt::WA_DontShowOnScreen);           // any size, never on the screen
    window.resize(1366, 705);
    window.show();
    settle();
    QDir().mkpath(shotDir());

    const QString label = TextSize::label(size);
    report("");
    report(QString("=== Text size: %1 (everything %2x) ===").arg(label).arg(factor));
    QElapsedTimer timer;
    timer.start();

    struct Look { QString name; Theme::Style style; Theme::Brightness bright; };
    const QList<Look> looks{{"soft light", Theme::Style::Clay, Theme::Brightness::Light},
                            {"soft dark", Theme::Style::Clay, Theme::Brightness::Dark},
                            {"high contrast light", Theme::Style::HighContrast, Theme::Brightness::Light},
                            {"high contrast dark", Theme::Style::HighContrast, Theme::Brightness::Dark}};
    const QList<Screen> all = screens();
    Findings sweep, named;
    int checked = 0;

    for (const Look &look : looks) {
        themes.setStyle(look.style);
        themes.setBrightness(look.bright);
        settle();
        for (const Screen &s : all) {
            window.resize(1366, 705);
            settle();
            if (!s.open(&window)) {
                report(QString("  (could not open %1)").arg(s.name));
                continue;
            }
            // The width sweep: Normal text, soft light only. Layout px are the same at every
            // text size (RZ1), so one sweep covers them all.
            if (size == TextSize::Size::Normal && look.style == Theme::Style::Clay
                && look.bright == Theme::Brightness::Light) {
                for (int h : {384, 705, 1017})
                    for (int w = Design::kMinWindowWidth; w <= 2560; w += 8) {
                        window.resize(w, h);
                        settle();
                        if (window.width() != w || window.height() != h) {
                            sweep.add(s.name, "Z5 window can't be this small",
                                      QString("smallest is %1 x %2").arg(window.minimumWidth()).arg(window.minimumHeight()),
                                      QString::number(w) + "@" + QString::number(h));
                            continue;
                        }
                        checkWindow(&window, s.name, QString::number(w) + "@" + QString::number(h), sweep);
                        ++checked;
                    }
            }
            // The named shapes, in every look at this text size.
            for (const Shape &shape : namedShapes()) {
                const int w = qRound(shape.w / factor), h = qRound(shape.h / factor);
                if (w < Design::kMinWindowWidth || h < Design::kMinWindowHeight)
                    continue;                                  // smaller than the smallest window
                window.resize(w, h);
                settle();
                if (window.width() != w || window.height() != h)
                    named.add(s.name, "Z5 window can't be this small",
                              QString("smallest is %1 x %2").arg(window.minimumWidth()).arg(window.minimumHeight()),
                              shape.name);
                checkWindow(&window, s.name, shape.name, named);
                ++checked;
                // Screenshots (at 1x, to keep the folder small): every screen maximized on
                // 1366 x 768; the key screens also at every shape, in every look and size.
                const bool her = shape.name == "maximized on 1366 x 768";
                const bool key = s.name == "Calendar: Month" || s.name == "Calendar: Day"
                              || s.name == "Editor" || s.name == "Settings";
                const bool normalLight = size == TextSize::Size::Normal && look.name == "soft light";
                if ((her && (normalLight || key)) || (key && normalLight)) {
                    QString file = QString("%1-%2-%3-%4x%5.png").arg(label, look.name, s.name).arg(w).arg(h);
                    file.replace(' ', '_').replace(':', "");
                    QImage img(window.size(), QImage::Format_RGB32);
                    window.render(&img);
                    img.save(shotDir() + "/" + file);
                }
            }
            s.close(&window);
        }
    }

    // --- Report -----------------------------------------------------------------------
    auto write = [](const Findings &f, bool widths) {
        QMap<QString, int> perRule;
        for (auto it = f.items().begin(); it != f.items().end(); ++it)
            perRule[it.key().rule] += 1;
        if (perRule.isEmpty()) {
            report("  Nothing found.");
            return;
        }
        for (auto it = perRule.begin(); it != perRule.end(); ++it)
            report(QString("  %1: %2").arg(it.key(), -32).arg(it.value()));
        QString lastScreen;
        for (auto it = f.items().begin(); it != f.items().end(); ++it) {
            if (it.key().screen != lastScreen) {
                lastScreen = it.key().screen;
                report("  " + lastScreen);
            }
            QString where;
            if (widths) {
                QMap<int, QList<int>> byHeight;
                for (const QString &x : it.value()) {
                    const QStringList wh = x.split('@');
                    byHeight[wh.value(1).toInt()] << wh.value(0).toInt();
                }
                QStringList parts;
                for (auto h = byHeight.begin(); h != byHeight.end(); ++h)
                    parts << QString("%1 high: %2 wide").arg(h.key()).arg(widthRanges(h.value()));
                where = parts.join("; ");
            } else {
                QStringList names(it.value().begin(), it.value().end());
                std::sort(names.begin(), names.end());
                where = names.join("; ");
            }
            report(QString("    [%1] %2  --  %3").arg(it.key().rule, it.key().what, where));
        }
    };
    if (size == TextSize::Size::Normal) {
        report("");
        report("Width sweep (soft light, 512 to 2560 wide in 8 px steps, 384 / 705 / 1017 high):");
        write(sweep, true);
    }
    report("");
    report("Named window shapes (all four looks; the shape's name, with its size in effective px):");
    write(named, false);
    report(QString("(%1 layouts checked in %2 s)").arg(checked).arg(timer.elapsed() / 1000.0, 0, 'f', 1));
    return 0;
}

} // namespace

int runLayoutCheck(const QStringList &args)
{
    const int at = args.indexOf("--layout-run");
    if (at >= 0)
        return runChild(args.value(at + 1));

    // Parent: one run per text size, each with its own scale (the program's Text size works
    // the same way: a restart with QT_SCALE_FACTOR).
    QFile::remove(reportPath());
    QDir(shotDir()).removeRecursively();
    report("Weather Tracker layout check (design-rules RZ8)");
    report("Date: " + QDateTime::currentDateTime().toString(Qt::ISODate));
    report("Sizes are in layout px (RZ1): effective pixels divided by the text size.");
    for (TextSize::Size s : TextSize::all()) {
        QProcess p;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_SCALE_FACTOR", QString::number(TextSize::factor(s), 'f', 2));
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::ForwardedChannels);
        p.start(QCoreApplication::applicationFilePath(),
                QStringList{"--layout", "--layout-run", TextSize::toSetting(s)});
        if (!p.waitForFinished(30 * 60 * 1000))
            report(QString("The %1 run did not finish.").arg(TextSize::label(s)));
        else if (p.exitStatus() == QProcess::CrashExit || p.exitCode() != 0)
            report(QString("The %1 run stopped with an error (code %2).").arg(TextSize::label(s)).arg(p.exitCode()));
    }
    report("");
    report("Done. Report: " + QDir::toNativeSeparators(reportPath()));
    report("Screenshots: " + QDir::toNativeSeparators(shotDir()));
    return 0;
}
