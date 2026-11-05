#include "MainWindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "AppLog.h"
#include "Artwork.h"
#include "Backups.h"
#include "ChartsView.h"
#include "CommandPanel.h"
#include "Confirm.h"
#include "Display.h"
#include "GoToDatePanel.h"
#include "PanelHost.h"
#include "SearchPanel.h"
#include "TourOverlay.h"
#include "DayView.h"
#include "EditorView.h"
#include "GuideDialog.h"
#include "MonthGridWidget.h"
#include "PeriodMath.h"
#include "PeriodSelector.h"
#include "RecordsView.h"
#include "Repository.h"
#include <QFontMetrics>
#include <functional>

#include "SettingsView.h"
#include "StatsView.h"
#include "Theme.h"
#include "ThemeManager.h"
#include "Toast.h"
#include "Version.h"
#include "WeekView.h"
#include "YearView.h"

namespace {

QString unitWord(Period p)          // lower-case, for "Previous month"
{
    return periodName(p).toLower();
}

QString goToText(Period p)
{
    return p == Period::Day ? QString("Go to today")
                            : QString("Go to this %1").arg(unitWord(p));
}

// Which stack slot shows which Calendar period. Slot 4 is the editor.
int calendarIndex(Period p)
{
    switch (p) {
    case Period::Day:   return 0;
    case Period::Week:  return 1;
    case Period::Month: return 2;
    case Period::Year:  return 3;
    default:            return 2;     // Season isn't offered on the Calendar tab
    }
}

// A QStackedWidget normally sizes itself to its LARGEST page. This makes every
// page except the visible one ignore its size, so only the shown page counts.
void sizeToCurrentPage(QStackedWidget *stack)
{
    auto apply = [stack] {
        for (int i = 0; i < stack->count(); ++i) {
            const auto policy = (i == stack->currentIndex()) ? QSizePolicy::Preferred
                                                             : QSizePolicy::Ignored;
            stack->widget(i)->setSizePolicy(policy, policy);
        }
    };
    QObject::connect(stack, &QStackedWidget::currentChanged, stack, apply);
    apply();
}

// How far (in pixels) the Add today's weather button is raised to line up with the tabs.
constexpr int kAddTodayLift = 18;

// QTabBar selects on press. This swallows the press and selects on release,
// only if the release is over the same tab that was pressed. It also tells the
// style which tab is down (property "pressedTab", -1 = none) so it can sink.
//
// Keyboard: when the tab bar has the focus, Left/Right/Home/End belong to it (the window
// would otherwise turn Left/Right into Previous/Next), and switch tabs the same animated
// way as a click.
class ReleaseToSelect : public QObject
{
public:
    explicit ReleaseToSelect(QTabBar *bar) : QObject(bar), m_bar(bar)
    {
        bar->setProperty("pressedTab", -1);
        bar->installEventFilter(this);
    }

    // Lets the window animate the switch: it receives the new index and the change, and runs it.
    std::function<void(int, const std::function<void()> &)> wrap;

protected:
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::ShortcutOverride || e->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(e);
            const int k = key->key();
            if ((key->modifiers() & ~Qt::KeypadModifier)
                || (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Home && k != Qt::Key_End))
                return false;
            if (e->type() == QEvent::ShortcutOverride) {
                e->accept();                        // "these keys are mine"
                return true;
            }
            select(target(k));
            return true;
        }
        if (e->type() != QEvent::MouseButtonPress
            && e->type() != QEvent::MouseButtonDblClick
            && e->type() != QEvent::MouseButtonRelease)
            return false;
        const auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() != Qt::LeftButton)
            return false;
        const int index = m_bar->tabAt(me->position().toPoint());
        if (e->type() == QEvent::MouseButtonRelease) {
            if (index >= 0 && index == m_pressed)
                select(index);
            m_pressed = -1;
        } else {
            m_pressed = index;
        }
        m_bar->setProperty("pressedTab", m_pressed);
        m_bar->update();
        return true;
    }

private:
    void select(int index)
    {
        if (index < 0 || index == m_bar->currentIndex() || !m_bar->isTabEnabled(index))
            return;
        auto change = [this, index] { m_bar->setCurrentIndex(index); };
        if (wrap)
            wrap(index, change);
        else
            change();
    }

    // The tab a key moves to: the next enabled one that way, or the first/last.
    int target(int key) const
    {
        const int n = m_bar->count();
        const int step = (key == Qt::Key_Left || key == Qt::Key_End) ? -1 : 1;
        int i = key == Qt::Key_Home ? 0 : key == Qt::Key_End ? n - 1 : m_bar->currentIndex() + step;
        while (i >= 0 && i < n && !m_bar->isTabEnabled(i))
            i += step;
        return (i >= 0 && i < n) ? i : -1;
    }

    QTabBar *m_bar;
    int m_pressed = -1;
};

// The root of the window. It paints the background (the clay pattern, or a plain fill in
// other looks) once, behind everything. The pages and views on top of it are transparent,
// so the background is the same on every screen.
class PageBackground : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        Artwork::paintPage(p, rect(), palette().color(QPalette::Window));
    }
};

} // namespace

MainWindow::MainWindow(Repository &repo, QWidget *parent)
    : QMainWindow(parent)
    , m_repo(repo)
    , m_state(new AppState(this))
{
    setWindowTitle(QString("Weather Tracker %1").arg(APP_VERSION));
    resize(1000, 650);

    // --- Title row: Settings and Help (top-right) ---
    auto *title = new QLabel("Weather Tracker");
    title->setStyleSheet("font-size: 20pt; font-weight: bold;");

    m_settingsButton = new QPushButton("Settings");
    m_settingsButton->setMinimumHeight(44);

    m_searchButton = new QPushButton("Search");
    m_searchButton->setMinimumHeight(44);
    m_goToDateButton = new QPushButton("Go to date");
    m_goToDateButton->setMinimumHeight(44);

    m_helpButton = new QPushButton("Help");
    m_helpButton->setCheckable(true);
    m_helpButton->setChecked(m_repo.setting("help_visible", "1") == "1");
    m_helpButton->setMinimumHeight(44);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(m_searchButton);
    titleRow->addWidget(m_goToDateButton);
    titleRow->addWidget(m_settingsButton);
    titleRow->addWidget(m_helpButton);

    m_helpLabel = new QLabel;
    m_helpLabel->setWordWrap(true);

    // --- Navigation bar ---
    m_prevButton = new QPushButton;
    m_nextButton = new QPushButton;
    m_todayButton = new QPushButton;
    m_dateLabel = new QLabel;
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setStyleSheet("font-size: 14pt;");
    for (auto *b : {m_prevButton, m_nextButton, m_todayButton}) {
        b->setMinimumHeight(48);
        QSizePolicy keep = b->sizePolicy();
        keep.setRetainSizeWhenHidden(true);   // hidden ("All time") buttons still hold their space
        b->setSizePolicy(keep);
    }

    auto *navBar = new QHBoxLayout;
    navBar->addWidget(m_prevButton);
    navBar->addWidget(m_dateLabel, 1);
    navBar->addWidget(m_nextButton);
    navBar->addWidget(m_todayButton);

    // Each button says which key does the same thing.
    m_prevButton->setToolTip("Go back (Left arrow key)");
    m_nextButton->setToolTip("Go forward (Right arrow key)");
    m_todayButton->setToolTip("Come back to today (T key)");
    m_searchButton->setToolTip("Find days by their notes or events (Ctrl+F)");
    m_goToDateButton->setToolTip("Jump to any date (Ctrl+G)");
    m_settingsButton->setToolTip("Units, text size, look, backups and privacy");
    m_helpButton->setToolTip("Show or hide the tips under the title. "
                             "Press F1 for the user guide, or Ctrl+K for every command.");

    // Created ONCE here; refresh functions only update them.
    m_backButton = new QPushButton;
    m_backButton->setMinimumHeight(48);
    m_backButton->hide();

    m_addTodayButton = new QPushButton;

    // --- Tabs, each with its own selector ---
    m_selectors[0] = new PeriodSelector("Show:",
        {Period::Day, Period::Week, Period::Month, Period::Year});
    m_selectors[1] = new PeriodSelector("Numbers for:",
        {Period::Day, Period::Month, Period::Season, Period::Year});
    m_selectors[2] = new PeriodSelector("Chart for:",
        {Period::Month, Period::Season, Period::Year});
    m_selectors[3] = new PeriodSelector("Records for:",
        {Period::Month, Period::Season, Period::Year, Period::All});

    m_tabs = new QTabWidget;
    auto *tabClicks = new ReleaseToSelect(m_tabs->tabBar());

    // Calendar page: selector on top, a stack of views below.
    auto *calendarPage = new QWidget;
    auto *calendarLayout = new QVBoxLayout(calendarPage);
    auto *dayView = new DayView(*m_state, m_repo);
    m_dayView = dayView;
    auto *weekView = new WeekView(*m_state, m_repo);
    auto *monthGrid = new MonthGridWidget(*m_state, m_repo);
    m_monthGrid = monthGrid;
    auto *yearView = new YearView(*m_state);
    m_editor = new EditorView(m_repo);

    calendarLayout->addWidget(m_selectors[0]);
    m_calendarStack = new QStackedWidget;
    m_calendarStack->addWidget(dayView);      // 0
    m_calendarStack->addWidget(weekView);     // 1
    m_calendarStack->addWidget(monthGrid);    // 2
    m_calendarStack->addWidget(yearView);     // 3
    m_calendarStack->addWidget(m_editor);     // 4
    sizeToCurrentPage(m_calendarStack);
    m_tabContent[0] = m_calendarStack;
    calendarLayout->addWidget(m_calendarStack, 1);
    m_tabs->addTab(calendarPage, "Calendar");

    // Analytics page: selector on top, the painted stats in a scroll area.
    auto *analyticsPage = new QWidget;
    auto *analyticsLayout = new QVBoxLayout(analyticsPage);
    analyticsLayout->addWidget(m_selectors[1]);
    auto *statsScroll = new QScrollArea;
    statsScroll->setWidgetResizable(true);
    statsScroll->setFrameShape(QFrame::NoFrame);
    statsScroll->setWidget(new StatsView(*m_state, m_repo));
    statsScroll->viewport()->setAutoFillBackground(false);   // let the background show through
    statsScroll->widget()->setAutoFillBackground(false);
    analyticsLayout->addWidget(statsScroll, 1);
    m_tabContent[1] = statsScroll;
    m_tabs->addTab(analyticsPage, "Analytics");

    // Charts page
    auto *chartsPage = new QWidget;
    auto *chartsLayout = new QVBoxLayout(chartsPage);
    chartsLayout->addWidget(m_selectors[2]);
    auto *chartsView = new ChartsView(*m_state, m_repo);
    chartsLayout->addWidget(chartsView, 1);
    m_tabContent[2] = chartsView;
    m_tabs->addTab(chartsPage, "Charts");

    // Records page: selector on top, the painted records in a scroll area.
    auto *recordsPage = new QWidget;
    auto *recordsLayout = new QVBoxLayout(recordsPage);
    recordsLayout->addWidget(m_selectors[3]);
    auto *recordsScroll = new QScrollArea;
    recordsScroll->setWidgetResizable(true);
    recordsScroll->setFrameShape(QFrame::NoFrame);
    auto *recordsView = new RecordsView(*m_state, m_repo);
    recordsScroll->setWidget(recordsView);
    recordsScroll->viewport()->setAutoFillBackground(false);
    recordsView->setAutoFillBackground(false);
    recordsLayout->addWidget(recordsScroll, 1);
    m_tabContent[3] = recordsScroll;
    m_tabs->addTab(recordsPage, "Records");
    // The button goes in a holder so its position can be nudged (kAddTodayLift pixels up).
    auto *addTodayHolder = new QWidget;
    auto *holderLayout = new QVBoxLayout(addTodayHolder);
    holderLayout->setContentsMargins(0, 0, 0, kAddTodayLift);
    holderLayout->setSpacing(0);
    holderLayout->addWidget(m_addTodayButton);
    m_tabs->setCornerWidget(addTodayHolder, Qt::TopRightCorner);   // same row as the tabs

    // --- Assemble the normal screen ---
    auto *layout = new QVBoxLayout;
    layout->addLayout(titleRow);
    layout->addWidget(m_helpLabel);
    layout->addLayout(navBar);
    layout->addWidget(m_backButton, 0, Qt::AlignLeft);
    layout->addWidget(m_tabs, 1);
    auto *central = new QWidget;
    central->setLayout(layout);

    // Two top-level pages: the normal screen and Settings.
    m_settings = new SettingsView(m_repo);
    m_pages = new QStackedWidget;
    m_pages->addWidget(central);              // 0: the normal screen
    m_pages->addWidget(m_settings);           // 1: Settings
    sizeToCurrentPage(m_pages);
    auto *root = new PageBackground;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_pages);
    setCentralWidget(root);
    m_panelHost = new PanelHost(root);            // Search and Go to date appear on top of the window
    m_panelHost->setUnderlying(m_pages);
    m_toast = new Toast(root);                    // "Deleted ... [Undo]" at the bottom
    m_transition = new ViewTransition(root);      // short fades and slides between views
    refreshMotion();
    m_transition->setFloating({m_toast});         // the undo bar stays above, not in the pictures
    tabClicks->wrap = [this](int index, const std::function<void()> &change) {
        animate(m_tabs->currentWidget(),
                index > m_tabs->currentIndex() ? ViewTransition::Kind::Forward
                                               : ViewTransition::Kind::Backward,
                change);
    };
    // Any other switch (keyboard, command menu, tour) ends a running animation at once,
    // so an old picture never lingers over a different tab or page.
    auto stopAnimation = [this] {
        if (!m_inAnimatedChange)
            m_transition->finish();
    };
    connect(m_tabs, &QTabWidget::currentChanged, this, stopAnimation);
    connect(m_pages, &QStackedWidget::currentChanged, this, stopAnimation);

    // --- Wiring: navigation ---
    connect(m_prevButton, &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Backward, [this] {
            const Period p = m_state->period(activeTab());
            m_state->setCurrentDate(PeriodMath::step(m_state->currentDate(), p, -1));
        });
    });
    connect(m_nextButton, &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Forward, [this] {
            const Period p = m_state->period(activeTab());
            m_state->setCurrentDate(PeriodMath::step(m_state->currentDate(), p, +1));
        });
        tourReport("next");
    });
    connect(m_todayButton, &QPushButton::clicked, this, [this] {
        animate(contentArea(), towards(QDate::currentDate()), [this] {
            m_state->clearBack();
            m_state->setCurrentDate(QDate::currentDate());
        });
    });

    for (int i = 0; i < 4; ++i) {
        const Tab tab = static_cast<Tab>(i);
        connect(m_selectors[i], &PeriodSelector::periodChosen, this,
                [this, tab](Period p) {
                    // Day, Week, Month, Year: a bigger period comes from the right.
                    const auto kind = int(p) > int(m_state->period(tab))
                        ? ViewTransition::Kind::Forward : ViewTransition::Kind::Backward;
                    animate(contentArea(), kind, [this, tab, p] {
                        m_state->clearBack();     // choosing a view starts fresh
                        m_state->setPeriod(tab, p);
                    });
                    if (tab == Tab::Calendar && p == Period::Week)
                        tourReport("week");
                });
    }

    // --- Wiring: drilling in and Back ---
    auto openOnCalendar = [this](Period p, const QDate &d) {
        animate(contentArea(), ViewTransition::Kind::Forward,      // deeper
                [this, p, d] { m_state->openCalendar(p, d); });
    };
    connect(monthGrid, &MonthGridWidget::dayActivated, this, [openOnCalendar, this](const QDate &d) {
        openOnCalendar(Period::Day, d);
        tourReport("dayOpened");
    });
    connect(monthGrid, &MonthGridWidget::weekActivated, this,
            [openOnCalendar](const QDate &d) { openOnCalendar(Period::Week, d); });
    connect(weekView, &WeekView::dayActivated, this, [openOnCalendar, this](const QDate &d) {
        openOnCalendar(Period::Day, d);
        tourReport("dayOpened");
    });
    connect(yearView, &YearView::monthActivated, this,
            [openOnCalendar](const QDate &d) { openOnCalendar(Period::Month, d); });
    connect(recordsView, &RecordsView::dayActivated, this, [this](const QDate &d) {
        animate(m_tabs->currentWidget(), ViewTransition::Kind::Forward, [this, d] {
            m_tabs->setCurrentIndex(static_cast<int>(Tab::Calendar));
            m_state->openCalendar(Period::Day, d);
        });
    });
    connect(m_backButton, &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Backward, [this] { m_state->goBack(); });
    });

    // --- Wiring: editor ---
    connect(dayView, &DayView::editRequested, this, [this](const QDate &d) {
        animate(m_calendarStack, ViewTransition::Kind::Forward, [this, d] {
            m_editor->open(d);
            m_editing = true;
            refreshNav();
        });
        tourReport("editorOpened");
    });
    connect(m_editor, &EditorView::finished, this, [this, monthGrid](bool saved) {
        animate(m_calendarStack, ViewTransition::Kind::Backward, [this, monthGrid, saved] {
            m_editing = false;
            if (m_backupPending)
                runBackup();                  // the midnight backup that was waiting
            if (saved)
                monthGrid->reloadData();      // Day and Week views reload when shown
            refreshNav();
        });
        if (!saved)
            tourReport("editorCancelled");
    });
    connect(m_addTodayButton, &QPushButton::clicked, this, &MainWindow::openTodayEditor);

    // --- Wiring: search and go to date, and the keyboard shortcuts ---
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::openSearch);
    connect(m_goToDateButton, &QPushButton::clicked, this, &MainWindow::openGoToDate);

    // Shortcuts work only on the normal screen, and never while editing.
    auto shortcut = [this](const QKeySequence &keys, std::function<void()> action) {
        auto *sc = new QShortcut(keys, this);
        m_shortcuts.append(sc);
        connect(sc, &QShortcut::activated, this, [this, action] {
            if (!m_editing && m_pages->currentIndex() == 0 && !m_panelHost->isOpen() && !m_tour)
                action();
        });
    };
    auto press = [](QPushButton *b) {
        return [b] { if (b->isVisible() && b->isEnabled()) b->click(); };
    };
    shortcut(QKeySequence(Qt::Key_Left), press(m_prevButton));
    shortcut(QKeySequence(Qt::Key_Right), press(m_nextButton));
    shortcut(QKeySequence(Qt::Key_T), press(m_todayButton));
    shortcut(QKeySequence::Find, press(m_searchButton));                    // Ctrl+F
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_G), press(m_goToDateButton));
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_K), [this] { openCommands(); });
    // While a panel is open the keys belong to the panel (typing, calendar arrows).
    connect(m_panelHost, &PanelHost::opened, this, [this] {
        for (QShortcut *sc : m_shortcuts) sc->setEnabled(false);
    });
    connect(m_panelHost, &PanelHost::closed, this, [this] {
        for (QShortcut *sc : m_shortcuts) sc->setEnabled(true);
        const int kind = m_panelKind;
        m_panelKind = 0;
        if (kind == 1) tourReport("searchClosed");
        else if (kind == 2) tourReport("gotoClosed");
    });

    // --- Wiring: settings ---
    connect(m_settingsButton, &QPushButton::clicked, this, [this] {
        m_settings->open();
        animate(m_pages, ViewTransition::Kind::Forward, [this] { m_pages->setCurrentIndex(1); });
        tourReport("settingsOpened");
    });
    connect(m_settings, &SettingsView::done, this, [this] {
        refreshMotion();                          // the Motion choice may have changed
        animate(m_pages, ViewTransition::Kind::Backward, [this] {
            m_pages->setCurrentIndex(0);
            refreshNav();
        });
        tourReport("settingsDone");
    });
    connect(m_settings, &SettingsView::guideRequested, this, &MainWindow::openGuide);
    connect(m_settings, &SettingsView::tourRequested, this, [this] {
        m_pages->setCurrentIndex(0);              // leave Settings first, then start the tour
        refreshNav();
        QTimer::singleShot(0, this, &MainWindow::openTour);
    });
    auto *helpKey = new QShortcut(QKeySequence::HelpContents, this);       // F1, from anywhere
    connect(helpKey, &QShortcut::activated, this, &MainWindow::openGuide);

    // --- Wiring: deleting a day, and Undo ---
    connect(dayView, &DayView::deleteRequested, this, &MainWindow::deleteDay);
    auto *undoKey = new QShortcut(QKeySequence::Undo, this);               // Ctrl+Z
    connect(undoKey, &QShortcut::activated, this, [this] {
        if (!m_editing)                       // in a text box, Ctrl+Z belongs to the text box
            m_toast->undo();
    });
    connect(m_settings, &SettingsView::dataErased, this, [this](const QString &safetyCopy) {
        reloadData();
        m_toast->showMessage("All weather data was erased. Your backups are still in the "
                             "backups folder.", [this, safetyCopy] {
            if (!m_repo.replaceWith(safetyCopy)) {
                QMessageBox::warning(this, "Weather Tracker",
                                     "Sorry, the data could not be put back.\n\n" + m_repo.lastError());
                return;
            }
            AppLog::write("Erasing all weather data was undone");
            reloadData();
            m_toast->showMessage("Your weather data is back.", nullptr);
        });
    });
    connect(m_editor, &EditorView::erasedOnSave, this,
            [this](const QDate &date, const QString &what, const Repository::DayRecord &before) {
        const QString day = date.toString("dddd, MMMM d, yyyy");
        m_toast->showMessage("Saved " + day + ", and removed " + what + ".",
                             [this, date, day, before] {
            if (!m_repo.restoreDay(date, before))
                QMessageBox::warning(this, "Weather Tracker",
                                     "Sorry, the day could not be put back completely.\n\n"
                                     + m_repo.lastError());
            reloadData();
            m_toast->showMessage(day + " is back the way it was.", nullptr);
        });
    });

    // --- Wiring: keep the display in step with the state ---
    connect(m_state, &AppState::currentDateChanged, this, &MainWindow::refreshNav);
    connect(m_state, &AppState::periodChanged, this, &MainWindow::refreshNav);
    connect(m_state, &AppState::backChanged, this, &MainWindow::refreshNav);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        refreshNav();
        refreshHelp();
    });
    connect(m_helpButton, &QPushButton::toggled, this, [this](bool on) {
        m_repo.setSetting("help_visible", on ? "1" : "0");
        refreshHelp();
    });

    // --- Theme: help text colour, and the Add today button ---
    auto restyle = [this] {
        const auto c = ThemeManager::instance()->colors();
        m_helpLabel->setStyleSheet(QString("color: %1;").arg(c.mutedText.name()));
    };
    restyle();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, restyle);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &MainWindow::refreshAddToday);

    refreshNav();
    refreshHelp();
    resize(1000, 650);                        // set again now that all pages exist

    // Remember the window: its size and place, and the tab she was on.
    const QByteArray geometry = QByteArray::fromBase64(m_repo.setting("window_geometry").toLatin1());
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const int lastTab = m_repo.setting("last_tab", "0").toInt();
    if (lastTab >= 0 && lastTab < m_tabs->count())
        m_tabs->setCurrentIndex(lastTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        m_repo.setSetting("last_tab", QString::number(index));
        tourReport(QString("tab%1").arg(index));
    });

    // Just restarted after "Restore from backup" or "Load a copy": offer Undo, which goes
    // back to the safety copy made before the data was replaced. Cleared at once, so the
    // offer is made only this one time.
    const QString undoReplace = m_repo.setting("undo_replace");
    if (!undoReplace.isEmpty()) {
        m_repo.removeSetting("undo_replace");
        if (QFileInfo::exists(undoReplace))
            QTimer::singleShot(0, this, [this, undoReplace] {
                m_toast->showMessage("Your weather data was replaced.", [this, undoReplace] {
                    if (!m_repo.replaceWith(undoReplace)) {
                        QMessageBox::warning(this, "Weather Tracker",
                                             "Sorry, the data could not be put back.\n\n"
                                             + m_repo.lastError());
                        return;
                    }
                    AppLog::write("Replacing the weather data was undone");
                    QMessageBox::information(this, "Weather Tracker",
                                             "Your data was put back. Weather Tracker will now restart.");
                    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
                    QCoreApplication::quit();
                });
            });
    }

    // The first time the program opens: a short tour (after the window is on screen).
    if (m_repo.setting("tour_done") != "1")
        QTimer::singleShot(0, this, &MainWindow::openTour);

    // Startup backup: only when there is none for today yet.
    if (Backups::autoBackupAllowed() && !Backups::existsFor(QDate::currentDate()))
        runBackup();

    // --- Overnight rollover: check the date once a minute ---
    m_dayTimer = new QTimer(this);
    m_dayTimer->setInterval(60 * 1000);
    connect(m_dayTimer, &QTimer::timeout, this, &MainWindow::checkNewDay);
    m_dayTimer->start();
}

Tab MainWindow::activeTab() const
{
    return static_cast<Tab>(m_tabs->currentIndex());
}

void MainWindow::checkNewDay()
{
    const QDate before = m_state->today();
    if (!m_state->refreshToday())
        return;

    // Follow to the new today only if she was looking at the old today and isn't editing.
    if (!m_editing && m_state->currentDate() == before)
        m_state->setCurrentDate(m_state->today());

    refreshNav();      // "Go to today" and "Add today's weather" depend on today

    // A new day started: back up now, or as soon as the editor is closed.
    if (m_editing)
        m_backupPending = true;
    else
        runBackup();
}

void MainWindow::runBackup()
{
    m_backupPending = false;
    if (!Backups::autoBackupAllowed())
        return;
    QString error;
    if (!Backups::backupNow(m_repo, &error))
        AppLog::write("Automatic backup failed: " + error);
}

void MainWindow::openGuide()
{
    if (m_panelHost->isOpen() || m_tour)
        return;
    GuideDialog dialog(this);
    dialog.exec();
}

void MainWindow::tourReport(const QString &event)
{
    if (m_tour)
        m_tour->report(event);
}

// Every tour step starts from the same screen: Calendar tab, Month view, today,
// nothing open. An open editor is dropped without saving.
void MainWindow::resetForTour()
{
    if (m_panelHost->isOpen())
        m_panelHost->closePanel();
    if (m_editing)
        m_editor->abandon();
    m_pages->setCurrentIndex(0);
    m_tabs->setCurrentIndex(static_cast<int>(Tab::Calendar));
    m_state->clearBack();
    m_state->setPeriod(Tab::Calendar, Period::Month);
    m_state->setCurrentDate(QDate::currentDate());
    refreshNav();
}

void MainWindow::openTour()
{
    if (m_tour)
        return;
    if (m_panelHost->isOpen())
        m_panelHost->closePanel();

    TourOverlay::Hooks hooks;
    hooks.targets = [this](int step) -> QList<TourTarget> {
        if (m_panelHost->isOpen())
            return {};                                // a panel is its own lit area
        switch (step) {
        case 1:
            if (m_editing)
                return {TourTarget{m_editor, false}, TourTarget{m_editor->cancelButton(), true}};
            return {TourTarget{m_addTodayButton, true}};
        case 2:
            return {TourTarget{m_selectors[0], true}, TourTarget{m_nextButton, true},
                    TourTarget{m_calendarStack, false}};
        case 3:
            return {TourTarget{m_tabs->tabBar(), true}};
        case 4:
            return {TourTarget{m_searchButton, true}, TourTarget{m_goToDateButton, true}};
        case 5:
            if (m_pages->currentIndex() == 1) {       // on Settings only "Done" is lit
                for (QPushButton *b : m_settings->findChildren<QPushButton *>())
                    if (b->text() == "Done")
                        return {TourTarget{b, true}};
                return {};
            }
            return {TourTarget{m_settingsButton, true}};
        default:
            return {};
        }
    };
    hooks.resetScreen = [this] { resetForTour(); };
    hooks.editorDirty = [this] { return m_editing && m_editor->hasUnsavedChanges(); };

    m_tour = new TourOverlay(centralWidget(), m_panelHost, hooks);
    m_editor->setSaveEnabled(false);                  // nothing is saved during the tour
    connect(m_panelHost, &PanelHost::opened, m_tour, &TourOverlay::cardToFront);
    connect(m_tour, &TourOverlay::finished, this, [this] {
        TourOverlay *tour = m_tour;
        m_tour = nullptr;
        tour->deleteLater();
        m_editor->setSaveEnabled(true);
        resetForTour();
        m_repo.setSetting("tour_done", "1");          // finished or skipped: do not show it again by itself
    });
    m_tour->start();
}

void MainWindow::openSearch()
{
    if (m_panelHost->isOpen())
        return;
    auto *panel = new SearchPanel(m_repo);
    connect(panel, &SearchPanel::closeRequested, m_panelHost, &PanelHost::closePanel);
    connect(panel, &SearchPanel::dayChosen, this, [this](const QDate &d) {
        m_panelHost->closePanel();
        animate(m_tabs->currentWidget(), ViewTransition::Kind::Forward, [this, d] {
            m_tabs->setCurrentIndex(static_cast<int>(Tab::Calendar));
            m_state->openCalendar(Period::Day, d);
        });
    });
    if (auto *input = panel->findChild<QLineEdit *>())
        connect(input, &QLineEdit::textEdited, this, [this] { tourReport("searchTyped"); });
    m_panelKind = 1;
    m_panelHost->showPanel(panel, QSize(760, 640));
    tourReport("searchOpened");
}

void MainWindow::openTodayEditor()
{
    m_state->clearBack();
    m_state->setPeriod(Tab::Calendar, Period::Day);
    m_state->setCurrentDate(QDate::currentDate());
    m_tabs->setCurrentIndex(static_cast<int>(Tab::Calendar));
    m_editor->open(QDate::currentDate());
    m_editing = true;
    refreshNav();
    tourReport("editorOpened");
}

void MainWindow::openCommands()
{
    if (m_panelHost->isOpen() || m_editing || m_tour)
        return;
    auto tab = [this](Tab t) {
        return [this, t] { m_tabs->setCurrentIndex(static_cast<int>(t)); };
    };
    auto show = [this](Period p) {
        return [this, p] {
            m_tabs->setCurrentIndex(static_cast<int>(Tab::Calendar));
            m_state->clearBack();
            m_state->setPeriod(Tab::Calendar, p);
        };
    };
    auto press = [](QPushButton *b) {
        return [b] { if (b->isVisible() && b->isEnabled()) b->click(); };
    };
    const bool recorded = m_repo.weatherFor(m_state->today()).has_value();
    QList<Command> commands = {
        {recorded ? "Edit today's weather" : "Add today's weather", "", "new write enter record",
         [this] { openTodayEditor(); }},
        {"Show one day", "", "calendar", show(Period::Day)},
        {"Show a week", "", "calendar", show(Period::Week)},
        {"Show a month", "", "calendar", show(Period::Month)},
        {"Show a year", "", "calendar", show(Period::Year)},
        {"Go back", "Left arrow", "previous earlier", press(m_prevButton)},
        {"Go forward", "Right arrow", "next later", press(m_nextButton)},
        {"Go to today", "T", "now this", press(m_todayButton)},
        {"Go to a date", "Ctrl+G", "jump find day", [this] { openGoToDate(); }},
        {"Search notes and events", "Ctrl+F", "find look", [this] { openSearch(); }},
        {"Undo the last delete", "Ctrl+Z", "back restore", [this] { m_toast->undo(); }},
        {"Open the Calendar tab", "", "", tab(Tab::Calendar)},
        {"Open the Analytics tab", "", "averages totals numbers", tab(Tab::Analytics)},
        {"Open the Charts tab", "", "graph lines", tab(Tab::Charts)},
        {"Open the Records tab", "", "hottest coldest streaks", tab(Tab::Records)},
        {"Back up now", "", "backup copy save", [this] {
             QString error;
             if (Backups::backupNow(m_repo, &error))
                 m_toast->showMessage("A backup was saved.", nullptr);
             else
                 QMessageBox::warning(this, "Weather Tracker",
                                      "Sorry, the backup could not be made.\n\n" + error);
         }},
        {"Open Settings", "", "options units text size look backup privacy",
         press(m_settingsButton)},
        {"Show or hide the tips", "", "help hints", [this] { m_helpButton->toggle(); }},
        {"Open the user guide", "F1", "help manual", [this] { openGuide(); }},
        {"Show the tour again", "", "help welcome", [this] { openTour(); }},
    };

    auto *panel = new CommandPanel(commands);
    connect(panel, &CommandPanel::closeRequested, m_panelHost, &PanelHost::closePanel);
    connect(panel, &CommandPanel::chosen, this, [this](const Command &command) {
        const auto run = command.run;
        m_panelHost->closePanel();
        QTimer::singleShot(0, this, run);         // after the menu has closed
    });
    m_panelKind = 3;
    m_panelHost->showPanel(panel, QSize(680, 620));
}

void MainWindow::openGoToDate()
{
    if (m_panelHost->isOpen())
        return;
    auto *panel = new GoToDatePanel(m_state->currentDate());
    connect(panel, &GoToDatePanel::cancelled, m_panelHost, &PanelHost::closePanel);
    connect(panel, &GoToDatePanel::dateChosen, this, [this](const QDate &d) {
        m_panelHost->closePanel();
        animate(contentArea(), towards(d), [this, d] {
            m_state->clearBack();
            m_state->setCurrentDate(d);           // stays on this tab and view
        });
    });
    m_panelKind = 2;
    m_panelHost->showPanel(panel, QSize(600, 600));
    tourReport("gotoOpened");
}

void MainWindow::refreshNav()
{
    const Period p = m_state->period(activeTab());
    const QDate date = m_state->currentDate();
    const QDate today = m_state->today();

    m_dateLabel->setText(PeriodMath::title(date, p));
    m_prevButton->setText("\u25C0 Previous " + unitWord(p));
    m_nextButton->setText("Next " + unitWord(p) + " \u25B6");
    m_todayButton->setText(goToText(p));

    // All three buttons get the same fixed width: the widest any of them needs, for any
    // period. Each text is tried on the real button, so the style's padding and the arrow
    // font are included. The text stays centered.
    int widest = 0;
    const QString keepPrev = m_prevButton->text();
    const QString keepNext = m_nextButton->text();
    const QString keepToday = m_todayButton->text();
    for (Period q : {Period::Day, Period::Week, Period::Month, Period::Season, Period::Year}) {
        m_prevButton->setText("\u25C0 Previous " + unitWord(q));
        m_nextButton->setText("Next " + unitWord(q) + " \u25B6");
        m_todayButton->setText(goToText(q));
        for (auto *b : {m_prevButton, m_nextButton, m_todayButton})
            widest = qMax(widest, b->sizeHint().width());
    }
    m_prevButton->setText(keepPrev);
    m_nextButton->setText(keepNext);
    m_todayButton->setText(keepToday);
    for (auto *b : {m_prevButton, m_nextButton, m_todayButton})
        b->setFixedWidth(widest);

    // While editing, navigation is locked so nothing can be lost.
    m_todayButton->setEnabled(!m_editing &&
        PeriodMath::startOf(date, p) != PeriodMath::startOf(today, p));
    m_prevButton->setEnabled(!m_editing);
    const bool showNav = p != Period::All;          // "All time" has nothing to step through
    for (auto *b : {m_prevButton, m_nextButton, m_todayButton})
        b->setVisible(showNav);
    m_nextButton->setEnabled(!m_editing);
    m_selectors[0]->setEnabled(!m_editing);
    m_settingsButton->setEnabled(!m_editing);
    m_searchButton->setEnabled(!m_editing);
    m_goToDateButton->setEnabled(!m_editing);
    m_tabs->tabBar()->setEnabled(!m_editing);

    for (int i = 0; i < 4; ++i)
        m_selectors[i]->setPeriod(m_state->period(static_cast<Tab>(i)));

    m_calendarStack->setCurrentIndex(
        m_editing ? 4 : calendarIndex(m_state->period(Tab::Calendar)));

    const auto back = m_state->backTarget();
    m_backButton->setVisible(back.has_value() && activeTab() == Tab::Calendar && !m_editing);
    if (back)
        m_backButton->setText("\u2190 Back to " + PeriodMath::title(back->date, back->period));

    refreshAddToday();
}

void MainWindow::refreshAddToday()
{
    const QDate today = m_state->today();
    const bool onTodaysDayView = activeTab() == Tab::Calendar
        && m_state->period(Tab::Calendar) == Period::Day
        && m_state->currentDate() == today;
    const bool show = activeTab() == Tab::Calendar && !m_editing && !onTodaysDayView;
    m_addTodayButton->parentWidget()->setVisible(show);
    m_addTodayButton->setVisible(show);
    if (!show)
        return;
    m_addTodayButton->setFixedHeight(m_tabs->tabBar()->sizeHint().height());   // same height as the tabs

    const bool recorded = m_repo.weatherFor(today).has_value()
        || !m_repo.note(today).trimmed().isEmpty()
        || !m_repo.events(today).isEmpty();

    const auto c = ThemeManager::instance()->colors();
    const bool contrast = ThemeManager::instance()->style() == Theme::Style::HighContrast;

    m_addTodayButton->setText(recorded ? "Edit today's weather \u2713"
                                       : "Add today's weather");

    // Same width whichever text is showing; measured on the real button. Text stays centered.
    {
        const QString keep = m_addTodayButton->text();
        m_addTodayButton->setFixedWidth(QWIDGETSIZE_MAX);          // release before measuring
        int addWidest = 0;
        for (const QString &t : {QString("Add today's weather"),
                                 QString("Edit today's weather \u2713")}) {
            m_addTodayButton->setText(t);
            addWidest = qMax(addWidest, m_addTodayButton->sizeHint().width());
        }
        m_addTodayButton->setText(keep);
        m_addTodayButton->setFixedWidth(addWidest + 24);   // slack: the bold style is applied after this
    }

    // Applying a stylesheet is slow, so only do it when the look actually changes.
    const QString key = QString("%1|%2|%3|%4")
                            .arg(contrast).arg(recorded)
                            .arg(c.accent.name(), c.accentText.name());
    if (m_addTodayButton->property("lookKey").toString() == key)
        return;
    m_addTodayButton->setProperty("lookKey", key);

    m_addTodayButton->setCheckable(false);          // a plain button in both styles
    if (contrast) {
        m_addTodayButton->setProperty("accentFill", false);
        if (recorded)
            m_addTodayButton->setStyleSheet(QString(
                "border: 3px solid %1; color: %1; background: transparent; "
                "border-radius: 4px;").arg(c.accent.name()));
        else
            m_addTodayButton->setStyleSheet(QString(
                "font-weight: bold; color: %1; background: %2; "
                "border: 3px solid %2; border-radius: 4px;")
                .arg(c.accentText.name(), c.accent.name()));
    } else {
        // Clay: no background or border in the stylesheet, so ClayStyle paints it.
        m_addTodayButton->setStyleSheet("font-weight: bold;");
        m_addTodayButton->setProperty("accentFill", !recorded);   // blue when nothing recorded
        m_addTodayButton->update();
    }
}

void MainWindow::refreshHelp()
{
    static const char *const hints[4] = {
        "Calendar: choose Day, Week, Month or Year with the \"Show:\" buttons. "
        "Use the Previous and Next buttons, or the Left and Right arrow keys, to move around. "
        "Press T for today. Press F1 for the user guide, or Ctrl+K for a list of everything "
        "you can do.",
        "Analytics: choose a period with the \"Numbers for:\" buttons to see "
        "averages and totals for it.",
        "Charts: choose Month, Season or Year to see temperature and "
        "precipitation drawn as charts.",
        "Records: the hottest, coldest and wettest days, streaks, and hot and cold "
        "day counts. Click a row with an arrow to open that day."
    };
    m_helpLabel->setText(hints[m_tabs->currentIndex()]);
    m_helpLabel->setVisible(m_helpButton->isChecked());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_editing && m_editor->hasUnsavedChanges()) {
        if (!Confirm::destructive(this, "Close without saving?",
                                  "You have changes that have not been saved.",
                                  "Keep editing", "Close without saving")) {
            event->ignore();                  // stay open
            return;
        }
    }
    m_repo.setSetting("window_geometry", QString::fromLatin1(saveGeometry().toBase64()));
    runBackup();                              // a last backup before closing
    event->accept();
}
// ---------------- Deleting a day ----------------

void MainWindow::deleteDay(const QDate &date)
{
    // Keep what was there, so Undo can put it back exactly.
    const Repository::DayRecord before = m_repo.dayRecord(date);
    if (!m_repo.deleteDay(date)) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, this day could not be deleted. Nothing was changed.\n\n"
                             + m_repo.lastError());
        return;
    }
    reloadData();
    const QString day = date.toString("dddd, MMMM d, yyyy");
    m_toast->showMessage("Deleted everything recorded for " + day + ".",
                         [this, date, day, before] {
        const bool ok = m_repo.restoreDay(date, before);
        reloadData();
        if (ok)
            m_toast->showMessage(day + " is back.", nullptr);
        else
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, the day could not be put back completely.\n\n"
                                 + m_repo.lastError());
    });
}

void MainWindow::reloadData()
{
    m_monthGrid->reloadData();
    m_dayView->reload();                      // the other views reload when they are shown
    refreshAddToday();
}

// ---------------- View transitions ----------------

void MainWindow::animate(QWidget *area, ViewTransition::Kind kind, const std::function<void()> &change)
{
    if (m_tour) {                             // the tour's own overlay must stay on top
        change();
        return;
    }
    m_transition->capture(area);
    if (m_toast->isVisible())
        m_toast->raise();                     // the undo bar stays above the fading picture
    m_inAnimatedChange = true;
    change();
    m_inAnimatedChange = false;
    m_transition->play(kind);
}

ViewTransition::Kind MainWindow::towards(const QDate &date) const
{
    return date >= m_state->currentDate() ? ViewTransition::Kind::Forward
                                          : ViewTransition::Kind::Backward;
}

QWidget *MainWindow::contentArea() const
{
    return m_tabContent[std::size_t(m_tabs->currentIndex())];
}

void MainWindow::refreshMotion()
{
    m_transition->setEnabled(Display::kViewAnimationsInThisVersion
                             && m_repo.setting("motion", "on") != "off"
                             && Display::animationsAllowed());
}
