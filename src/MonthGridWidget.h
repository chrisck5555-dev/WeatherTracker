#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>

#include "WeatherDay.h"

class AppState;
class Repository;

// Draws the whole month grid itself: no child widgets, one paintEvent.
class MonthGridWidget : public QWidget
{
    Q_OBJECT

public:
    MonthGridWidget(AppState &state, Repository &repo, QWidget *parent = nullptr);

    void reloadData();      // re-read markers from the database (call after saving)

signals:
    void dayActivated(const QDate &date);       // clicked (pressed and released) on a day
    void weekActivated(const QDate &weekStart); // clicked "Open week"

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize minimumSizeHint() const override { return QSize(720, 420); }

private:
    void onDateChanged();
    QDate gridStart() const;                    // first cell: a Sunday
    QRectF gridArea() const;                    // the 6x7 cells, excluding header/legend
    QRectF weekButtonRect(int row) const;
    QDate dateAt(const QPointF &pos) const;     // invalid QDate if outside the cells

    AppState &m_state;
    Repository &m_repo;
    QDate m_loadedStart;                        // which grid the cached data belongs to
    QMap<QDate, WeatherDay> m_weather;
    QSet<QDate> m_noteDates;
    QSet<QDate> m_eventDates;
    int m_pressWeek = -1;                       // "Open week" row being pressed
    QDate m_pressDate;                          // day cell being pressed
};