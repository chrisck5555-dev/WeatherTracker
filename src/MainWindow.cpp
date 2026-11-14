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
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
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
#include "UndoBar.h"
#include "Version.h"
#include "WeekView.h"
#include "YearView.h"
#include "ChoiceButtons.h"
#include "FloatingBars.h"
#include "FlowLayout.h"
#include "Glass.h"
#include "Icons.h"
#include "PageArea.h"
#include "PageHeader.h"
#include "Motion.h"
#include "Typography.h"

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

// A stack whose size is only its current page's. QStackedWidget asks every page for its
// height at a given width, hidden ones too, so a tall hidden page (the editor) would make
// the page scroll for nothing.
class PageStack : public QStackedWidget
{
public:
    using QStackedWidget::QStackedWidget;
    bool hasHeightForWidth() const override
    {
        const QWidget *w = currentWidget();
        return w && w->hasHeightForWidth();
    }
    int heightForWidth(int width) const override
    {
        const QWidget *w = currentWidget();
        return w && w->hasHeightForWidth() ? w->heightForWidth(width) : -1;
    }
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

// The normal screen: the page with the bars floating over it. The window places them itself
// (layoutFrame).
class FrameWidget : public QWidget
{
public:
    using QWidget::QWidget;
    std::function<void()> onResize;

protected:
    void resizeEvent(QResizeEvent *) override { if (onResize) onResize(); }
};

QPushButton *toolbarButton(const QString &text, const QString &icon, const QString &tip)
{
    auto *b = new QPushButton(text);
    b->setIcon(Icons::icon(icon));
    b->setToolTip(tip);
    return b;
}

} // namespace

MainWindow::MainWindow(Repository &repo, QWidget *parent)
    : QMainWindow(parent)
    , m_repo(repo)
    , m_state(new AppState(this))
{
    setWindowTitle(QString("Weather Tracker %1").arg(APP_VERSION));
    resize(Design::kCheckedWindowWidth, Design::kCheckedWindowHeight);

    // The frame, built in reading order so Tab goes bars, header, page (KB1).
    auto *frame = new FrameWidget;
    m_frame = frame;
    m_pageArea = new PageArea;                      // placed later; the bars need it now
    m_backdrop = new GlassBackdrop(m_frame, m_pageArea);
    m_bars = new FloatingBars(m_backdrop, m_pageArea, m_frame);

    // --- Toolbar (plan 2.1): the daily action alone, then Search and Go to date, then
    // Settings and Help, each group its own glass shape (G7) ---
    m_addTodayButton = new QPushButton;
    m_addTodayButton->setProperty("wtKind", "primary");
    m_addGroup = m_bars->addGroup(FloatingBars::Toolbar);
    m_addGroup->flow()->addWidget(m_addTodayButton);

    m_searchButton = toolbarButton("Search", "search", "Find days by their notes or events (Ctrl+F)");
    m_goToDateButton = toolbarButton("Go to date", "calendar_arrow_right", "Jump to any date (Ctrl+G)");
    m_findGroup = m_bars->addGroup(FloatingBars::Toolbar, true);
    m_findGroup->flow()->addWidget(m_searchButton);
    m_findGroup->flow()->addWidget(m_goToDateButton);

    m_settingsButton = toolbarButton("Settings", "settings", "Units, text size, look, backups and privacy");
    m_helpButton = toolbarButton("Help", "question_circle", "Open the user guide (F1)");   // D34
    m_appGroup = m_bars->addGroup(FloatingBars::Toolbar);
    m_appGroup->flow()->addWidget(m_settingsButton);
    m_appGroup->flow()->addWidget(m_helpButton);

    // --- Tab row: the tabs, then this tab's period choice at the right end (L10) ---
    m_tabButtons = new ChoiceButtons;
    m_tabButtons->setAccessibleName("Tabs");
    m_tabButtons->addChoice(int(Tab::Calendar), "Calendar", "calendar_ltr");
    m_tabButtons->addChoice(int(Tab::Analytics), "Analytics", "table");
    m_tabButtons->addChoice(int(Tab::Charts), "Charts", "data_line");
    m_tabButtons->addChoice(int(Tab::Records), "Records", "trophy");
    m_tabGroup = m_bars->addGroup(FloatingBars::TabRow);
    m_tabGroup->flow()->addWidget(m_tabButtons);

    m_selectors[0] = new PeriodSelector("Show:",
        {Period::Day, Period::Week, Period::Month, Period::Year});
    m_selectors[1] = new PeriodSelector("Numbers for:",
        {Period::Day, Period::Month, Period::Season, Period::Year});
    m_selectors[2] = new PeriodSelector("Chart for:",
        {Period::Month, Period::Season, Period::Year});
    m_selectors[3] = new PeriodSelector("Records for:",
        {Period::Month, Period::Season, Period::Year, Period::All});
    for (int i = 0; i < 4; ++i) {
        m_selectorGroups[i] = m_bars->addGroup(FloatingBars::TabRow, true);
        m_selectorGroups[i]->flow()->addWidget(m_selectors[i]);
        m_selectorGroups[i]->setVisible(i == 0);
    }

    // --- The page: its header, then the tab's view ---
    m_pageColumn = new QWidget;
    auto *column = new QVBoxLayout(m_pageColumn);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(Design::kGapCards);
    m_header = new PageHeader;
    column->addWidget(m_header);
    m_tabStack = new PageStack;
    column->addWidget(m_tabStack, 1);

    auto *dayView = new DayView(*m_state, m_repo);
    m_dayView = dayView;
    auto *weekView = new WeekView(*m_state, m_repo);
    auto *monthGrid = new MonthGridWidget(*m_state, m_repo);
    m_monthGrid = monthGrid;
    auto *yearView = new YearView(*m_state, m_repo);
    m_editor = new EditorView(m_repo);
    m_calendarStack = new PageStack;
    m_calendarStack->addWidget(dayView);      // 0
    m_calendarStack->addWidget(weekView);     // 1
    m_calendarStack->addWidget(monthGrid);    // 2
    m_calendarStack->addWidget(yearView);     // 3
    m_calendarStack->addWidget(m_editor);     // 4
    sizeToCurrentPage(m_calendarStack);
    // Save and Cancel stay pinned at the bottom of the window while the form scrolls
    // (plan 2.4, SC4); the frame places them (layoutFrame), after the form in Tab order.
    m_editorFooter = m_editor->footer();
    m_editorFooter->setParent(m_frame);
    m_editorFooter->hide();
    m_tabContent[0] = m_calendarStack;
    m_tabStack->addWidget(m_calendarStack);

    // The page itself scrolls (SC1), so Analytics and Records sit straight on it.
    auto *statsView = new StatsView(*m_state, m_repo);
    statsView->setAutoFillBackground(false);
    m_tabContent[1] = statsView;
    m_tabStack->addWidget(statsView);
    auto *chartsView = new ChartsView(*m_state, m_repo);
    m_tabContent[2] = chartsView;
    m_tabStack->addWidget(chartsView);
    auto *recordsView = new RecordsView(*m_state, m_repo);
    recordsView->setAutoFillBackground(false);
    m_tabContent[3] = recordsView;
    m_tabStack->addWidget(recordsView);
    sizeToCurrentPage(m_tabStack);

    m_pageArea->setParent(m_frame);
    m_pageArea->setPage(m_pageColumn);
    m_pageArea->bar()->setParent(m_frame);
    m_bars->raise();
    frame->onResize = [this] { layoutFrame(); };
    connect(m_pageArea, &PageArea::barShownChanged, this, [this] { layoutFrame(); });

    // Two top-level pages: the normal screen and Settings (D40: without the bars).
    m_settings = new SettingsView(m_repo);
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_frame);              // 0: the normal screen
    m_pages->addWidget(m_settings);           // 1: Settings
    sizeToCurrentPage(m_pages);
    auto *root = new PageBackground;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_pages);
    setCentralWidget(root);
    QWidget::setTabOrder(m_editor->lastField(), m_editor->saveButton());   // now in one window
    QWidget::setTabOrder(m_editor->saveButton(), m_editor->cancelButton());
    // The footer is solid: the window's own background, as if the page weren't there.
    m_editor->footer()->paintBackground = [root](QPainter &p, QWidget *w) {
        p.translate(-QPointF(w->mapTo(root, QPoint(0, 0))));
        Artwork::paintPage(p, QRectF(root->rect()), w->palette().color(QPalette::Window));
    };
    m_panelHost = new PanelHost(root);            // Search and Go to date appear on top of the window
    m_panelHost->setUnderlying(m_pages);
    m_transition = new ViewTransition(root);      // short fades and slides between views
    refreshMotion();
    // K1: over both pages (erasing everything happens in Settings), above everything else
    // (E1), and last, so Tab reaches it last.
    m_undoBar = new UndoBar(m_backdrop, root);
    // K1 (Chris, 2026-11-10): the bar floats over the bottom of the page and nothing moves.
    // While it shows, its glass frosts the page under it, and a control that gets the
    // keyboard focus is scrolled clear of it when the page can scroll (SC5).
    connect(m_undoBar, &UndoBar::spaceChanged, this, [this](int space) {
        m_backdrop->setPageUnderGlass(space > 0);
        m_undoSpace = space;
        m_pageArea->setFocusClearanceBottom(m_editing ? EditorFooter::footerHeight() : space);
        m_undoBar->update();
    });
    connect(m_pageArea->verticalScrollBar(), &QScrollBar::valueChanged, m_undoBar,
            qOverload<>(&QWidget::update));
    connect(m_backdrop, &GlassBackdrop::changed, m_undoBar, qOverload<>(&QWidget::update));
    connect(m_pages, &QStackedWidget::currentChanged, this, [this](int page) {
        m_undoBar->setFloating(page == 0);        // the glass has no copy of Settings: solid
        m_undoBar->update();
    });
    m_transition->setFloating({m_undoBar, m_bars});   // they stay above, not in the pictures
    auto stopAnimation = [this] {
        if (!m_inAnimatedChange)
            m_transition->finish();
    };
    connect(m_tabStack, &QStackedWidget::currentChanged, this, stopAnimation);
    connect(m_pages, &QStackedWidget::currentChanged, this, stopAnimation);

    // --- Wiring: the tabs ---
    connect(m_tabButtons, &ChoiceButtons::choiceMade, this, [this](int index) {
        if (index == m_tabStack->currentIndex())
            return;
        animate(m_tabStack->currentWidget(),
                index > m_tabStack->currentIndex() ? ViewTransition::Kind::Forward
                                                   : ViewTransition::Kind::Backward,
                [this, index] { setActiveTab(static_cast<Tab>(index)); });
    });

    // --- Wiring: navigation ---
    QPushButton *prev = m_header->previousButton();
    QPushButton *next = m_header->nextButton();
    QPushButton *today = m_header->todayButton();
    connect(prev, &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Backward, [this] {
            const Period p = m_state->period(activeTab());
            m_state->setCurrentDate(PeriodMath::step(m_state->currentDate(), p, -1));
        });
    });
    connect(next, &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Forward, [this] {
            const Period p = m_state->period(activeTab());
            m_state->setCurrentDate(PeriodMath::step(m_state->currentDate(), p, +1));
        });
        tourReport("next");
    });
    connect(today, &QPushButton::clicked, this, [this] {
        animate(contentArea(), towards(QDate::currentDate()), [this] {
            m_state->clearBack();
            m_state->setCurrentDate(QDate::currentDate());
        });
    });
    // CAL2: the title zooms out one level: Day and Week to their month, Month to its year.
    connect(m_header, &PageHeader::zoomOutRequested, this, [this] {
        const Period p = m_state->period(Tab::Calendar);
        const Period out = p == Period::Month ? Period::Year : Period::Month;
        animate(contentArea(), ViewTransition::Kind::Backward, [this, out] {
            m_state->clearBack();
            m_state->setPeriod(Tab::Calendar, out);
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
    connect(weekView, &WeekView::dayActivated, this, [openOnCalendar, this](const QDate &d) {
        openOnCalendar(Period::Day, d);
        tourReport("dayOpened");
    });
    connect(yearView, &YearView::monthActivated, this,
            [openOnCalendar](const QDate &d) { openOnCalendar(Period::Month, d); });
    connect(recordsView, &RecordsView::dayActivated, this, [this](const QDate &d) {
        animate(m_tabStack->currentWidget(), ViewTransition::Kind::Forward, [this, d] {
            setActiveTab(Tab::Calendar);
            m_state->openCalendar(Period::Day, d);
        });
    });
    connect(m_header->backButton(), &QPushButton::clicked, this, [this] {
        animate(contentArea(), ViewTransition::Kind::Backward, [this] { m_state->goBack(); });
    });

    // --- Wiring: editor ---
    connect(dayView, &DayView::editRequested, this, [this](const QDate &d) {
        animate(m_calendarStack, ViewTransition::Kind::Forward, [this, d] { openEditor(d); });
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
    shortcut(QKeySequence(Qt::Key_Left), press(prev));
    shortcut(QKeySequence(Qt::Key_Right), press(next));
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_T), press(today));                  // KB9
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_N), [this] { openTodayEditor(); });  // D41
    shortcut(QKeySequence::Find, press(m_searchButton));                         // Ctrl+F
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_G), press(m_goToDateButton));
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_K), [this] { openCommands(); });
    // KB7: F6 moves between the toolbar, the tab row and the page (also while editing).
    auto *f6 = new QShortcut(QKeySequence(Qt::Key_F6), this);
    connect(f6, &QShortcut::activated, this, [this] { focusArea(+1); });
    auto *shiftF6 = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F6), this);
    connect(shiftF6, &QShortcut::activated, this, [this] { focusArea(-1); });
    m_shortcuts << f6 << shiftF6;
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

    // --- Wiring: settings and help ---
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
    connect(m_helpButton, &QPushButton::clicked, this, &MainWindow::openGuide);
    auto *helpKey = new QShortcut(QKeySequence::HelpContents, this);       // F1, from anywhere
    connect(helpKey, &QShortcut::activated, this, &MainWindow::openGuide);

    // KB6: Ctrl+S saves while editing, wherever the focus is in the form. Esc does nothing
    // there (KB4): it only closes floating things.
    auto *saveKey = new QShortcut(QKeySequence::Save, this);
    connect(saveKey, &QShortcut::activated, this, [this] {
        if (m_editing && m_pages->currentIndex() == 0 && !m_panelHost->isOpen())
            m_editor->requestSave();
    });

    // --- Wiring: deleting a day, and Undo ---
    connect(dayView, &DayView::deleteRequested, this, &MainWindow::deleteDay);
    auto *undoKey = new QShortcut(QKeySequence::Undo, this);               // Ctrl+Z
    connect(undoKey, &QShortcut::activated, this, [this] {
        if (!m_editing)                       // in a text box, Ctrl+Z belongs to the text box
            m_undoBar->undo();
    });
    connect(m_settings, &SettingsView::dataErased, this, [this](const QString &safetyCopy) {
        reloadData();
        m_undoBar->showMessage("All weather data was erased. Your backups are still in the "
                               "backups folder.", [this, safetyCopy] {
            if (!m_repo.replaceWith(safetyCopy)) {
                QMessageBox::warning(this, "Weather Tracker",
                                     "Sorry, the data could not be put back.\n\n" + m_repo.lastError());
                return;
            }
            AppLog::write("Erasing all weather data was undone");
            reloadData();
            m_undoBar->showMessage("Your weather data is back.", nullptr);
        });
    });
    connect(m_editor, &EditorView::erasedOnSave, this,
            [this](const QDate &date, const QString &what, const Repository::DayRecord &before) {
        const QString day = date.toString("dddd, MMMM d, yyyy");
        m_undoBar->showMessage("Saved " + day + ", and removed " + what + ".",
                               [this, date, day, before] {
            if (!m_repo.restoreDay(date, before))
                QMessageBox::warning(this, "Weather Tracker",
                                     "Sorry, the day could not be put back completely.\n\n"
                                     + m_repo.lastError());
            reloadData();
            m_undoBar->showMessage(day + " is back the way it was.", nullptr);
        });
    });

    // --- Wiring: keep the display in step with the state ---
    connect(m_state, &AppState::currentDateChanged, this, &MainWindow::refreshNav);
    connect(m_state, &AppState::periodChanged, this, &MainWindow::refreshNav);
    connect(m_state, &AppState::backChanged, this, &MainWindow::refreshNav);
    connect(m_tabStack, &QStackedWidget::currentChanged, this, &MainWindow::refreshNav);

    // --- Theme: the glass copies and the Add today button ---
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_backdrop->invalidateBackground();
        refreshAddToday();
        layoutFrame();
        m_frame->update();
    });

    refreshNav();
    resize(Design::kCheckedWindowWidth, Design::kCheckedWindowHeight);   // set again now that all pages exist

    // Remember the window: its size and place, and the tab she was on.
    const QByteArray geometry = QByteArray::fromBase64(m_repo.setting("window_geometry").toLatin1());
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const int lastTab = m_repo.setting("last_tab", "0").toInt();
    if (lastTab >= 0 && lastTab < m_tabStack->count())
        setActiveTab(static_cast<Tab>(lastTab));
    connect(m_tabStack, &QStackedWidget::currentChanged, this, [this](int index) {
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
                m_undoBar->showMessage("Your weather data was replaced.", [this, undoReplace] {
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

// ---------------- The frame ----------------

void MainWindow::layoutFrame()
{
    const int w = m_frame->width(), h = m_frame->height();
    if (w <= 0 || h <= 0 || !m_undoBar)
        return;
    m_undoBar->place();                                // K1: over the page, which never moves
    const int barsH = m_bars->heightFor(w);
    // G14: the bars float while they take at most a third of the window's height.
    const bool floating = barsH <= h / 3;
    if (floating != m_bars->isFloating() || (floating && m_bars->parentWidget() != m_frame)) {
        if (floating) {
            m_bars->setParent(m_frame);
            m_pageArea->barsSlot()->hide();
        } else {
            m_pageArea->barsSlot()->layout()->addWidget(m_bars);
            m_pageArea->barsSlot()->show();
        }
        m_bars->setFloating(floating);
        m_bars->setVisible(!m_editing);
    }
    const int sbw = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_pageArea->bar());
    const bool barShown = !m_pageArea->bar()->isHidden();
    const int right = barShown ? std::max(Design::kWindowMargin, sbw) : Design::kWindowMargin;
    m_pageArea->setGeometry(0, 0, w, h);
    // While editing, Save and Cancel are pinned at the bottom and the page ends above them.
    const int footerH = m_editing ? EditorFooter::footerHeight() : 0;
    m_editorFooter->setVisible(m_editing);
    if (m_editing) {
        m_editorFooter->setGeometry(0, h - footerH, w, footerH);
        static_cast<EditorFooter *>(m_editorFooter)->setRightMargin(right);
        m_editorFooter->raise();
    }
    m_pageArea->setFocusClearanceBottom(m_editing ? footerH : m_undoSpace);   // SC5
    // D64: while editing, the editor has the whole window, as Settings does (D40). The bars
    // are locked then anyway, and this gives the form their 148 px.
    m_bars->setVisible(!m_editing);
    if (!floating)
        m_pageArea->barsSlot()->setVisible(!m_editing);
    if (m_editing) {
        m_pageArea->setPageMargins(Design::kWindowMargin, right, footerH);
        m_pageArea->setFocusClearance(0);
        m_pageArea->bar()->setGeometry(w - sbw, 0, sbw, h);
    } else if (floating) {
        m_bars->setGeometry(0, 0, w, barsH);
        m_pageArea->setPageMargins(barsH, right, footerH);  // L9: 16 below the tab row
        m_pageArea->setFocusClearance(m_bars->barsBottom());
        const int top = barsH - Design::kBarToPage;        // the scroll bar starts below the tab row
        m_pageArea->bar()->setGeometry(w - sbw, top, sbw, std::max(0, h - top));
        m_bars->raise();
    } else {
        m_pageArea->setPageMargins(0, right, footerH);
        m_pageArea->setFocusClearance(0);
        m_pageArea->bar()->setGeometry(w - sbw, 0, sbw, h);
    }
    m_pageArea->bar()->raise();
    m_undoBar->raise();
    if (m_frame->size() != m_backdropSize) {          // G11: the background's copy fits the window
        m_backdropSize = m_frame->size();
        m_backdrop->invalidateBackground();
    }
    // Changing the window's minimum from inside its own resize can leave the page behind,
    // so it waits for the event loop.
    QTimer::singleShot(0, this, &MainWindow::updateMinimumSize);
}

void MainWindow::updateMinimumSize()
{
    // Z5: 636 x 320 layout px. A page that can't reflow that far yet (its new layout comes
    // in a later delivery) holds the window as wide as it needs, so nothing is ever cut off.
    const int sbw = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_pageArea->bar());
    const int page = m_pageColumn->minimumSizeHint().width() + Design::kWindowMargin
                   + std::max(Design::kWindowMargin, sbw);
    const QSize wanted(std::max(Design::kMinWindowWidth, page), Design::kMinWindowHeight);
    if (minimumSize() != wanted)
        setMinimumSize(wanted);
}

void MainWindow::focusArea(int step)
{
    if (m_pages->currentIndex() != 0 || m_panelHost->isOpen() || m_tour)
        return;
    // The first control of each area she can reach.
    auto firstIn = [](QWidget *area) -> QWidget * {
        if (auto *choices = qobject_cast<ChoiceButtons *>(area))
            for (QAbstractButton *b : choices->buttons())
                if (b->isChecked() && b->isEnabled() && b->isVisible())
                    return b;
        for (QWidget *w : area->findChildren<QWidget *>())
            if (w->isVisible() && w->isEnabled() && (w->focusPolicy() & Qt::TabFocus))
                return w;
        return nullptr;
    };
    const QList<QWidget *> areas{m_bars, m_tabButtons, m_pageColumn};
    auto targetOf = [&](int i) -> QWidget * {
        if (i == 0) {
            for (GlassGroup *g : {m_addGroup, m_findGroup, m_appGroup})
                if (QWidget *w = firstIn(g))
                    return w;
            return nullptr;
        }
        return firstIn(areas[i]);
    };
    int current = 2;
    if (QWidget *f = focusWidget()) {
        if (m_tabButtons->isAncestorOf(f) || m_tabGroup->isAncestorOf(f))
            current = 1;
        else if (m_bars->isAncestorOf(f))
            current = m_tabGroup->isAncestorOf(f) ? 1 : 0;
        for (GlassGroup *g : m_selectorGroups)
            if (g->isAncestorOf(f))
                current = 1;
    }
    for (int n = 1; n <= 3; ++n) {
        const int i = ((current + step * n) % 3 + 3) % 3;
        if (QWidget *w = targetOf(i)) {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
}

bool MainWindow::focusNextPrevChild(bool next)
{
    // KB8: on the main screen Tab stops at the last control instead of going round again.
    // The undo bar floats outside the frame, so the chain is taken over the whole window area.
    QWidget *now = focusWidget();
    QWidget *area = centralWidget();
    if (now && area && m_pages->currentIndex() == 0 && area->isAncestorOf(now)) {
        QList<QWidget *> chain;
        QWidget *w = area;
        do {
            w = w->nextInFocusChain();
            if (area->isAncestorOf(w) && w->isVisible() && w->isEnabled()
                && (w->focusPolicy() & Qt::TabFocus))
                chain << w;
        } while (w != area && chain.size() < 2000);
        if (!chain.isEmpty() && ((next && now == chain.last()) || (!next && now == chain.first())))
            return true;
    }
    return QMainWindow::focusNextPrevChild(next);
}

void MainWindow::changeEvent(QEvent *event)
{
    // G9: Windows' transparency and battery saver may have changed while she was away.
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        Glass::refreshSystem();
        m_bars->update();
        m_undoBar->update();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setActiveTab(Tab tab)
{
    m_tabStack->setCurrentIndex(static_cast<int>(tab));
    m_tabButtons->setChosen(static_cast<int>(tab));
}

Tab MainWindow::activeTab() const
{
    return static_cast<Tab>(m_tabStack->currentIndex());
}

void MainWindow::checkNewDay()
{
    Glass::refreshSystem();                   // G9: battery saver may have come on
    m_bars->update();
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
    setActiveTab(Tab::Calendar);
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
            return {TourTarget{m_selectorGroups[0], true}, TourTarget{m_header->nextButton(), true},
                    TourTarget{m_calendarStack, false}};
        case 3:
            return {TourTarget{m_tabGroup, true}};
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
        animate(m_tabStack->currentWidget(), ViewTransition::Kind::Forward, [this, d] {
            setActiveTab(Tab::Calendar);
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
    if (m_editing)
        return;
    m_state->clearBack();
    m_state->setPeriod(Tab::Calendar, Period::Day);
    m_state->setCurrentDate(QDate::currentDate());
    setActiveTab(Tab::Calendar);
    openEditor(QDate::currentDate());
    tourReport("editorOpened");
}

void MainWindow::openEditor(const QDate &date)
{
    m_undoBar->forget();                      // K1: a new change starts; the old undo goes
    m_editor->open(date);
    m_editing = true;
    refreshNav();
}

void MainWindow::openCommands()
{
    if (m_panelHost->isOpen() || m_editing || m_tour)
        return;
    auto tab = [this](Tab t) {
        return [this, t] { setActiveTab(t); };
    };
    auto show = [this](Period p) {
        return [this, p] {
            setActiveTab(Tab::Calendar);
            m_state->clearBack();
            m_state->setPeriod(Tab::Calendar, p);
        };
    };
    auto press = [](QPushButton *b) {
        return [b] { if (b->isVisible() && b->isEnabled()) b->click(); };
    };
    const bool recorded = m_repo.weatherFor(m_state->today()).has_value();
    QList<Command> commands = {
        {recorded ? "Edit today's weather" : "Add today's weather", "Ctrl+N", "new write enter record",
         [this] { openTodayEditor(); }},
        {"Show one day", "", "calendar", show(Period::Day)},
        {"Show a week", "", "calendar", show(Period::Week)},
        {"Show a month", "", "calendar", show(Period::Month)},
        {"Show a year", "", "calendar", show(Period::Year)},
        {"Go back", "Left arrow", "previous earlier", press(m_header->previousButton())},
        {"Go forward", "Right arrow", "next later", press(m_header->nextButton())},
        {"Go to today", "Ctrl+T", "now this", press(m_header->todayButton())},
        {"Go to a date", "Ctrl+G", "jump find day", [this] { openGoToDate(); }},
        {"Search notes and events", "Ctrl+F", "find look", [this] { openSearch(); }},
        {"Open the Calendar tab", "", "", tab(Tab::Calendar)},
        {"Open the Analytics tab", "", "averages totals numbers", tab(Tab::Analytics)},
        {"Open the Charts tab", "", "graph lines", tab(Tab::Charts)},
        {"Open the Records tab", "", "hottest coldest streaks", tab(Tab::Records)},
        {"Back up now", "", "backup copy save", [this] {
             QString error;
             if (Backups::backupNow(m_repo, &error))
                 m_undoBar->showMessage("A backup was saved.", nullptr);
             else
                 QMessageBox::warning(this, "Weather Tracker",
                                      "Sorry, the backup could not be made.\n\n" + error);
         }},
        {"Open Settings", "", "options units text size look backup privacy",
         press(m_settingsButton)},
        {"Open the user guide", "F1", "help manual", [this] { openGuide(); }},
        {"Show the tour again", "", "help welcome", [this] { openTour(); }},
    };

    if (m_undoBar->canUndo())                 // K1: the last action stays undoable
        commands.insert(1, {"Undo: " + m_undoBar->undoLabel(), "Ctrl+Z", "back restore put back",
                            [this] { m_undoBar->undo(); }});
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
    const Tab tab = activeTab();
    const Period p = m_state->period(tab);
    const QDate date = m_state->currentDate();
    const QDate today = m_state->today();
    const QString unit = unitWord(p);

    // The header (plan 2.1). The title zooms out on the Calendar's Day, Week and Month (CAL2).
    const bool zoomable = tab == Tab::Calendar && !m_editing
        && (p == Period::Day || p == Period::Week || p == Period::Month);
    const QString zoomTip = zoomable ? (p == Period::Month ? "Show the whole year" : "Show the whole month")
                                     : QString();
    m_header->setPeriodTitle(date, p, zoomTip);           // T5: in columns, so it doesn't jump
    QPushButton *prev = m_header->previousButton();
    QPushButton *next = m_header->nextButton();
    QPushButton *todayButton = m_header->todayButton();
    prev->setToolTip(QString("Previous %1 (Left arrow)").arg(unit));
    next->setToolTip(QString("Next %1 (Right arrow)").arg(unit));
    const bool onToday = PeriodMath::startOf(date, p) == PeriodMath::startOf(today, p);
    todayButton->setToolTip(!onToday ? goToText(p) + " (Ctrl+T)"
                            : p == Period::Day ? QString("You are already on today (Ctrl+T)")
                                               : QString("You are already on this %1 (Ctrl+T)").arg(unit));   // ST5
    m_header->setNavigationShown(p != Period::All);       // "All time" has nothing to step through
    m_header->setVisible(!m_editing);                     // the editor has its own heading (plan 2.4)

    // While editing, the bars and the header are locked so nothing can be lost.
    todayButton->setEnabled(!m_editing && !onToday);
    prev->setEnabled(!m_editing);
    next->setEnabled(!m_editing);
    for (GlassGroup *g : {m_addGroup, m_findGroup, m_appGroup, m_tabGroup})
        g->setEnabled(!m_editing);
    for (int i = 0; i < 4; ++i) {
        m_selectorGroups[i]->setEnabled(!m_editing);
        m_selectorGroups[i]->setVisible(i == static_cast<int>(tab));
        m_selectors[i]->setPeriod(m_state->period(static_cast<Tab>(i)));
    }
    m_tabButtons->setChosen(static_cast<int>(tab));

    m_calendarStack->setCurrentIndex(
        m_editing ? 4 : calendarIndex(m_state->period(Tab::Calendar)));

    const auto back = m_state->backTarget();
    const bool showBack = back.has_value() && tab == Tab::Calendar && !m_editing;
    m_header->setBack(showBack ? "Back to " + PeriodMath::title(back->date, back->period) : QString());

    refreshAddToday();
    m_backdrop->invalidate();                 // G11: the page changed
    layoutFrame();                            // this tab's choice may change the bars' size
}

void MainWindow::refreshAddToday()
{
    const QDate today = m_state->today();
    const bool recorded = m_repo.weatherFor(today).has_value()
        || !m_repo.note(today).trimmed().isEmpty()
        || !m_repo.events(today).isEmpty();

    // The main daily action: always the primary button (B1, B2), with its own icon (I3).
    const QString addText = QStringLiteral("Add today's weather");
    const QString editText = QStringLiteral("Edit today's weather");
    m_addTodayButton->setText(recorded ? editText : addText);
    m_addTodayButton->setIcon(Icons::icon(recorded ? "calendar_edit" : "calendar_add"));
    m_addTodayButton->setToolTip(QString("%1 (Ctrl+N)").arg(recorded ? "Change what you recorded today"
                                                                     : "Record today's weather"));

    // Same width whichever text is showing, so the groups beside it don't move.
    const QString keep = m_addTodayButton->text();
    m_addTodayButton->setFixedWidth(QWIDGETSIZE_MAX);              // release before measuring
    int widest = 0;
    for (const QString &t : {addText, editText}) {
        m_addTodayButton->setText(t);
        widest = qMax(widest, m_addTodayButton->sizeHint().width());
    }
    m_addTodayButton->setText(keep);
    m_addTodayButton->setFixedWidth(widest);
    m_addTodayButton->update();
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
    m_undoBar->showMessage("Deleted everything recorded for " + day + ".",
                         [this, date, day, before] {
        const bool ok = m_repo.restoreDay(date, before);
        reloadData();
        if (ok)
            m_undoBar->showMessage(day + " is back.", nullptr);
        else
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, the day could not be put back completely.\n\n"
                                 + m_repo.lastError());
    });
}

void MainWindow::reloadData()
{
    m_backdrop->invalidate();                 // G11: new data on the page
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
    if (m_undoBar->isVisible())
        m_undoBar->raise();                   // the undo bar stays above the fading picture
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
    return m_tabContent[std::size_t(m_tabStack->currentIndex())];
}

void MainWindow::refreshMotion()
{
    Motion::setUserEnabled(m_repo.setting("motion", "on") != "off");   // M19
    m_transition->setEnabled(Display::kViewAnimationsInThisVersion
                             && m_repo.setting("motion", "on") != "off"
                             && Display::animationsAllowed());
}
