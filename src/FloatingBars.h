#pragma once

#include <QList>
#include <QPointer>
#include <QWidget>

class FlowLayout;
class GlassBackdrop;
class GlassGroup;
class QScrollArea;

// The toolbar and the tab row (G14, L9): two rows of glass groups that float above the
// page. Each row is a flow of whole groups (G7): a group that doesn't fit moves to the next
// line, and is never split.
//
// While floating it also draws SC4's soft edge: a band from the window's top to 16 px below
// the tab row where the page shows only as its blurred, frosted copy, fading out over the
// last 16 px. With solid glass (G9) it is a hard edge: solid, with E2's line along its
// bottom. The mouse wheel over the bars scrolls the page beneath them.
class FloatingBars : public QWidget
{
    Q_OBJECT

public:
    FloatingBars(GlassBackdrop *backdrop, QScrollArea *area, QWidget *parent = nullptr);

    enum Row { Toolbar = 0, TabRow = 1 };
    // A new glass group at the end of `row`. `pushRight`: it and the groups after it sit at
    // the right end of their line.
    GlassGroup *addGroup(Row row, bool pushRight = false);

    // The bars' height for a window `width` wide: 16 + the rows + 16 (L9).
    int heightFor(int width) const;
    int barsBottom() const;           // where the tab row's last line ends
    // G14: false while the bars scroll with the page (they'd take over a third of the window).
    bool isFloating() const { return m_floating; }
    void setFloating(bool floating);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    GlassBackdrop *m_backdrop;
    QPointer<QScrollArea> m_area;
    QWidget *m_rows[2];
    FlowLayout *m_flows[2];
    QList<GlassGroup *> m_groups;
    bool m_floating = true;
};
