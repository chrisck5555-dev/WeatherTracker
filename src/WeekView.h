#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>

#include "Units.h"
#include "WeatherDay.h"

class AppState;
class HoverFade;
class Repository;

// Seven painted rows, Sunday first, then the key (K9). One widget, no per-row children.
// - Each row is a button at least 44 high (TB6), drawn like the month's tiles (CAL1).
// - Columns, the same in every week so nothing moves as she steps (T5): the day, its picture,
//   high, low, precipitation, the markers.
// - When one line doesn't fit, each row takes two: the day, picture and markers, then the
//   numbers below (RZ9's Narrow and narrow Medium windows).
class WeekView : public QWidget
{
    Q_OBJECT

public:
    WeekView(AppState &state, Repository &repo, QWidget *parent = nullptr);

    bool twoLines() const { return twoLines(width()); }   // for the layout check
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
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;                 // claims Up/Down
    void keyPressEvent(QKeyEvent *event) override;      // Up/Down move a day, Enter opens it
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    struct Columns {                    // x of each column inside a row's padding
        double picture, high, low, precip, markers;
        double cellHigh, cellLow, cellPrecip;           // their widths
        double line1, line2;            // what one line or the second line needs
    };
    Columns columns(bool twoLines) const;
    bool twoLines(int width) const;
    int rowHeight(bool twoLines) const;
    int keyHeight(int width) const;
    void reload();
    void updateAccessibleText();
    QDate weekStart() const;
    QRectF rowRect(int row) const;
    int rowAt(const QPointF &pos) const;

    AppState &m_state;
    Repository &m_repo;
    QDate m_loadedStart;
    Units::System m_units = Units::System::Metric;
    QMap<QDate, WeatherDay> m_weather;
    QSet<QDate> m_noteDates, m_eventDates;
    int m_pressed = -1;                         // row being pressed
    HoverFade *m_hover = nullptr;               // ST2's tint under the mouse
};
