#pragma once

#include <QList>
#include <QMainWindow>
#include <QSize>
#include <array>

#include "AppState.h"      // needs the Tab enum
#include "ViewTransition.h"
#include <functional>

class ChoiceButtons;
class DayView;
class EditorView;
class FloatingBars;
class GlassBackdrop;
class GlassGroup;
class MonthGridWidget;
class PageArea;
class PageHeader;
class PanelHost;
class PeriodSelector;
class QCloseEvent;
class QPushButton;
class QShortcut;
class QStackedWidget;
class QTimer;
class Repository;
class SettingsView;
class TourOverlay;
class UndoBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Repository &repo, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    Tab activeTab() const;
    void setActiveTab(Tab tab);
    void refreshNav();
    void refreshAddToday();
    void layoutFrame();                 // places the bars, page, scroll bar and undo bar
    void updateMinimumSize();           // Z5, plus what the current page still needs
    void focusArea(int step);           // F6 / Shift+F6: toolbar, tab row, page (KB7)
    void checkNewDay();
    void runBackup();
    void openGuide();
    void openTour();
    void openSearch();
    void openGoToDate();
    void openCommands();                // Ctrl+K: every action, with its key
    void openTodayEditor();             // "Add today's weather"
    void openEditor(const QDate &date);
    void resetForTour();                // the known screen each tour step starts from
    void tourReport(const QString &event);
    void deleteDay(const QDate &date);  // with an Undo in the undo bar
    void reloadData();                  // after data changed outside the editor
    // Runs `change` with a short animation over `area` (none when Motion is off).
    void animate(QWidget *area, ViewTransition::Kind kind, const std::function<void()> &change);
    QWidget *contentArea() const;       // the part of the current tab that changes with the date
    ViewTransition::Kind towards(const QDate &date) const;   // later = forward
    void refreshMotion();               // Settings or Windows may have turned animations off

    Repository &m_repo;
    AppState *m_state;

    // The window frame (G14, K1): the page, the bars floating above it, the undo bar.
    QWidget *m_frame = nullptr;
    PageArea *m_pageArea = nullptr;
    FloatingBars *m_bars = nullptr;
    GlassBackdrop *m_backdrop = nullptr;
    UndoBar *m_undoBar = nullptr;
    GlassGroup *m_addGroup = nullptr, *m_findGroup = nullptr, *m_appGroup = nullptr,
               *m_tabGroup = nullptr;
    std::array<GlassGroup *, 4> m_selectorGroups{};
    ChoiceButtons *m_tabButtons = nullptr;
    PageHeader *m_header = nullptr;
    QWidget *m_pageColumn = nullptr;
    QStackedWidget *m_tabStack = nullptr;
    QSize m_backdropSize;               // the frame size the glass background was made for

    QPushButton *m_addTodayButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_helpButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_goToDateButton = nullptr;
    DayView *m_dayView = nullptr;
    MonthGridWidget *m_monthGrid = nullptr;
    ViewTransition *m_transition = nullptr;
    bool m_inAnimatedChange = false;    // true while animate() runs its change
    std::array<QWidget *, 4> m_tabContent{};   // per tab: what changes when the date or period does
    QStackedWidget *m_calendarStack = nullptr;
    QStackedWidget *m_pages = nullptr;
    SettingsView *m_settings = nullptr;
    EditorView *m_editor = nullptr;
    QWidget *m_editorFooter = nullptr;  // Save and Cancel, pinned at the window's bottom
    int m_undoSpace = 0;                // what the undo bar covers at the bottom (K1)
    QTimer *m_dayTimer = nullptr;
    PanelHost *m_panelHost = nullptr;
    TourOverlay *m_tour = nullptr;
    int m_panelKind = 0;                // 0 none, 1 search, 2 go to date, 3 commands
    QList<QShortcut *> m_shortcuts;
    bool m_editing = false;
    bool m_backupPending = false;       // midnight passed while the editor was open
    std::array<PeriodSelector *, 4> m_selectors{};
};
