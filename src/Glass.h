#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <QWidget>

class FlowLayout;
class QPainter;
class QScrollArea;

// Glass for the parts that float above the page (G1): the toolbar, the tab row and the undo
// bar. The app paints its own glass, so it looks the same on Windows 10 and 11 (G11).
namespace Glass {

// G9: glass is drawn solid (the glass colour at 100%) in the high contrast looks, when
// Windows' "Transparency effects" is off, on battery saver, or when drawing it was too slow.
bool solid();
void refreshSystem();                 // re-reads Windows' settings (window activated, each minute)
double frost();                       // G3: 85% in soft light, 90% in soft dark

// G11: how long glass took in one frame while something moved. Three frames over 4 ms in
// a row turn glass solid for the rest of the session.
void reportFrameCost(double ms);

// E1: the soft shadow under a floating shape (none in high contrast). `elevation` is
// Design::kElevationBar (16), kElevationPanel (32) or kElevationDialog (128).
void drawShadow(QPainter &p, const QRectF &shape, double radius, int elevation);

} // namespace Glass

// What passes under the glass, blurred once and reused (G11):
// - the window's background, redone when the window's size or the look changes;
// - the scrolling page, redone 150 ms after it changes (another page, new data, text size,
//   look or window size), and only while some of it is under the glass.
// Scrolling only moves the page's copy.
class GlassBackdrop : public QObject
{
    Q_OBJECT

public:
    // `frame`: the widget the bars float in. `area`: the page's scroll area.
    GlassBackdrop(QWidget *frame, QScrollArea *area);

    void invalidate();                // the page changed
    void invalidateBackground();      // the window's size or the look changed
    // While the undo bar floats over the page, the page is under glass even when it isn't
    // scrolled (the bars only have it under them once it has scrolled).
    void setPageUnderGlass(bool on);

    // Paints the blurred backdrop for `frameRect` (in the frame's coordinates) at `target`
    // in `p`. The page's copy is drawn only where the page is under the glass.
    void paint(QPainter &p, const QRectF &target, const QRect &frameRect);

    // The page's blurred copy (premultiplied, quarter size) and where its top-left is in the
    // frame, for the scroll edge (SC4). Null when the page isn't under the bars.
    const QImage &pageCopy();
    QPoint pageOrigin() const;
    QWidget *frame() const { return m_frame; }
    static constexpr double kScale = 0.25;   // G11: drawn at quarter size

signals:
    void changed();                   // a new copy is ready: the glass repaints

private:
    void makePageCopy();
    void makeBackgroundCopy();

    QPointer<QWidget> m_frame;
    QPointer<QScrollArea> m_area;
    QImage m_page, m_background;
    bool m_pageStale = true, m_backgroundStale = true;
    bool m_pageUnderGlass = false;
    QTimer m_settle;
};

// One glass shape holding a group of controls (G7): 8 px from its edge to the controls and
// 8 px between them (L9, S2). Its parent draws its shadow (Glass::drawShadow), because a
// shadow falls outside the shape.
class GlassGroup : public QWidget
{
public:
    GlassGroup(GlassBackdrop *backdrop, QWidget *parent = nullptr);

    FlowLayout *flow() const { return m_flow; }
    // False while the bars scroll with the page (G14): there is nothing under them to frost,
    // so the shape is drawn in the glass colour, solid.
    void setFloating(bool floating);
    double radius() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    GlassBackdrop *m_backdrop;
    FlowLayout *m_flow;
    bool m_floating = true;
};

namespace Glass {
// Paints one glass shape filling `shape` in `w` (backdrop, frost, edge), or solid (G9).
void paintShape(QPainter &p, QWidget *w, const QRectF &shape, double radius,
                GlassBackdrop *backdrop, bool floating);
}
