#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>

#include "Units.h"
#include "WeatherDay.h"

class HoverFade;
class Repository;

// "This week" at the bottom of the Day view (D63): the viewed day's week, Sunday to Saturday,
// as seven tiles drawn like the month's (CAL1). Each shows its weekday and date, picture,
// high / low and markers, centred. Clicking one opens that day.
// - Seven across in Wide and Medium windows; four, then three, in Narrow ones (RZ9).
// - The keyboard already moves day by day there (Left and Right), so the tiles take no focus.
class WeekStrip : public QWidget
{
    Q_OBJECT

public:
    explicit WeekStrip(Repository &repo, QWidget *parent = nullptr);

    void setWeek(const QDate &viewed, Units::System units);   // loads the week of `viewed`

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void dayActivated(const QDate &date);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;                 // tooltips

private:
    int columns() const;
    QRectF tileRect(int index) const;   // 0..6
    int tileAt(const QPointF &pos) const;
    QString describe(int index) const;

    Repository &m_repo;
    QDate m_viewed, m_start;
    Units::System m_units = Units::System::Metric;
    QMap<QDate, WeatherDay> m_weather;
    QSet<QDate> m_noteDates, m_eventDates;
    int m_pressed = -1;
    HoverFade *m_hover = nullptr;
};