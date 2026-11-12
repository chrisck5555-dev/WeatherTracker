#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>

#include "WeatherDay.h"

class AppState;
class HoverFade;
class Repository;

// Draws the whole month itself: no child widgets, one paintEvent.
// - Weekday names above the columns (CAL5), six weeks of seven equal tiles (CAL3), the key
//   below (K9).
// - The tiles' contents follow CAL4: the first arrangement that fits every tile.
// - Its height follows its width (heightForWidth): narrow tiles stack their contents and grow
//   taller, and the page scrolls.
class MonthGridWidget : public QWidget
{
    Q_OBJECT

public:
    MonthGridWidget(AppState &state, Repository &repo, QWidget *parent = nullptr);

    void reloadData();      // re-read markers from the database (call after saving)

    // CAL4's arrangements, in the order they are tried.
    enum class Arrangement { TwoRows, OneRow, Stacked };
    struct TileLayout {
        Arrangement arrangement = Arrangement::Stacked;
        bool picture = true;            // false: dropped for lack of room (CAL4)
        bool highLow = false;
        int markersPerRow = 3;          // 0: dropped
    };
    TileLayout tileLayout(double tileWidth, double tileHeight) const;
    Arrangement arrangement() const;    // for the current size (tests and the layout check)

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void dayActivated(const QDate &date);       // clicked (pressed and released) on a day

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;                 // claims the arrow keys; tooltips
    void keyPressEvent(QKeyEvent *event) override;      // arrows move a day, Enter opens it
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void onDateChanged();
    QDate gridStart() const;                    // first tile: a Sunday
    double tileWidth(double width) const;
    double minTileHeight(double tileWidth) const;   // the least CAL4 allows at that width
    int keyHeight(double width) const;
    QRectF gridArea() const;                    // the 6 x 7 tiles, without names and key
    QRectF tileRect(int index) const;           // 0..41, on whole pixels
    QDate dateAt(const QPointF &pos) const;     // invalid QDate between or outside the tiles
    int hoverItemAt(const QPointF &pos) const;  // 0..41, or -1
    void updateAccessibleText();                // what a screen reader says: the selected day
    QString describe(const QDate &date) const;  // Format::describeDay for one tile
    void paintTile(QPainter &p, int index, const TileLayout &layout);

    AppState &m_state;
    Repository &m_repo;
    QDate m_loadedStart;                        // which grid the cached data belongs to
    QMap<QDate, WeatherDay> m_weather;
    QSet<QDate> m_noteDates;
    QSet<QDate> m_eventDates;
    QDate m_pressDate;                          // tile being pressed
    HoverFade *m_hover = nullptr;               // ST2's tint under the mouse
};
