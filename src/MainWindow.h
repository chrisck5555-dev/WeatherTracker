#pragma once

#include <QList>
#include <QMainWindow>
#include <array>

#include "AppState.h"      // needs the Tab enum

class DayView;
class EditorView;
class MonthGridWidget;
class Toast;
class PanelHost;
class TourOverlay;
class PeriodSelector;
class QCloseEvent;
class QLabel;
class QPushButton;
class QShortcut;
class QStackedWidget;
class QTabWidget;
class QTimer;
class Repository;
class SettingsView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Repository &repo, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Tab activeTab() const;
    void refreshNav();
    void refreshHelp();
    void refreshAddToday();
    void checkNewDay();
    void runBackup();
    void openGuide();
    void openTour();
    void openSearch();
    void openGoToDate();
    void resetForTour();                // the known screen each tour step starts from
    void tourReport(const QString &event);
    void deleteDay(const QDate &date);  // with an Undo in the toast
    void reloadData();                  // after data changed outside the editor

    Repository &m_repo;
    AppState *m_state;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_dateLabel = nullptr;
    QLabel *m_helpLabel = nullptr;
    QPushButton *m_prevButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QPushButton *m_todayButton = nullptr;
    QPushButton *m_helpButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_addTodayButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_goToDateButton = nullptr;
    Toast *m_toast = nullptr;
    DayView *m_dayView = nullptr;
    MonthGridWidget *m_monthGrid = nullptr;
    QStackedWidget *m_calendarStack = nullptr;
    QStackedWidget *m_pages = nullptr;
    SettingsView *m_settings = nullptr;
    EditorView *m_editor = nullptr;
    QTimer *m_dayTimer = nullptr;
    PanelHost *m_panelHost = nullptr;
    TourOverlay *m_tour = nullptr;
    int m_panelKind = 0;                // 0 none, 1 search, 2 go to date
    QList<QShortcut *> m_shortcuts;
    bool m_editing = false;
    bool m_backupPending = false;       // midnight passed while the editor was open
    std::array<PeriodSelector *, 4> m_selectors{};
};