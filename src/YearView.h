#pragma once

#include <QDate>
#include <QWidget>

#include "Stats.h"
#include "Units.h"

class AppState;
class HoverFade;
class Repository;

// Twelve painted month tiles, then the key (K9). Clicking one opens that month (CAL2).
// - 4 x 3 in Wide windows, 3 x 4 in Medium, 2 x 6 in Narrow (RZ9).
// - Each tile: in the soft looks, the season's picture across its top (at most 40% of the
//   tile, IL1); the month's name with the season's beside it; the average high and low and
//   the number of days recorded.
// - States as the month's tiles (CAL1); this month has the Today outline.
class YearView : public QWidget
{
    Q_OBJECT

public:
    YearView(AppState &state, Repository &repo, QWidget *parent = nullptr);

    int columns() const;                  // for the layout check
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void monthActivated(const QDate &dateInMonth);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;                 // claims the arrow keys
    void keyPressEvent(QKeyEvent *event) override;      // arrows move a month, Enter opens it
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    int columnsFor() const;
    int minTileHeight() const;
    int keyHeight(int width) const;
    QRectF tileRect(int index) const;     // 0..11
    int tileAt(const QPointF &pos) const;
    void reload();
    void updateAccessibleText();
    QString describe(int month) const;    // what Narrator reads for a month

    AppState &m_state;
    Repository &m_repo;
    int m_loadedYear = 0;
    Units::System m_units = Units::System::Metric;
    Stats::Summary m_months[12];
    int m_pressed = -1;                   // tile being pressed
    HoverFade *m_hover = nullptr;         // ST2's tint under the mouse
};
