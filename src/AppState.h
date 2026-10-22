#pragma once

#include <QDate>
#include <QObject>
#include <array>
#include <optional>

#include "Period.h"

enum class Tab { Calendar = 0, Analytics = 1, Charts = 2, Records = 3 };

// The screen "← Back" returns to.
struct BackTarget {
    Period period;
    QDate date;
};

class AppState : public QObject
{
    Q_OBJECT

public:
    explicit AppState(QObject *parent = nullptr);

    QDate currentDate() const { return m_currentDate; }
    void setCurrentDate(const QDate &date);

    Period period(Tab tab) const { return m_periods[static_cast<int>(tab)]; }
    void setPeriod(Tab tab, Period p);

    // Drill in (e.g. Month -> Day): remember where we came from, then go there.
    void openCalendar(Period p, const QDate &date);
    std::optional<BackTarget> backTarget() const { return m_back; }
    void goBack();
    void clearBack();

    QDate today() const { return m_today; }
    // Re-reads the system date. Returns true if the day changed.
    bool refreshToday();

signals:
    void currentDateChanged();
    void periodChanged(Tab tab);
    void backChanged();
    void todayChanged();

private:
    QDate m_currentDate;
    std::array<Period, 4> m_periods;
    std::optional<BackTarget> m_back;
    QDate m_today;
};