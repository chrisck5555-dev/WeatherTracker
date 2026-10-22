#include "AppState.h"

AppState::AppState(QObject *parent)
    : QObject(parent)
    , m_currentDate(QDate::currentDate())
    , m_periods{{Period::Month, Period::Month, Period::Month, Period::Month}}
    , m_today(QDate::currentDate())
{
}

void AppState::setCurrentDate(const QDate &date)
{
    if (date == m_currentDate)
        return;                      // nothing changed, so don't wake the listeners
    m_currentDate = date;
    emit currentDateChanged();
}

bool AppState::refreshToday()
{
    const QDate now = QDate::currentDate();
    if (now == m_today)
        return false;
    m_today = now;
    emit todayChanged();
    return true;
}

void AppState::setPeriod(Tab tab, Period p)
{
    auto &slot = m_periods[static_cast<int>(tab)];   // a reference into the array
    if (slot == p)
        return;
    slot = p;
    emit periodChanged(tab);
}

void AppState::openCalendar(Period p, const QDate &date)
{
    m_back = BackTarget{period(Tab::Calendar), m_currentDate};   // snapshot BEFORE moving
    setPeriod(Tab::Calendar, p);
    setCurrentDate(date);
    emit backChanged();
}

void AppState::goBack()
{
    if (!m_back)
        return;
    const BackTarget target = *m_back;     // copy first: reset() destroys the original
    m_back.reset();
    setPeriod(Tab::Calendar, target.period);
    setCurrentDate(target.date);
    emit backChanged();
}

void AppState::clearBack()
{
    if (!m_back)
        return;
    m_back.reset();
    emit backChanged();
}