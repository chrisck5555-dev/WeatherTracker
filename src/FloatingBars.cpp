#include "FloatingBars.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "Artwork.h"
#include "ClayPaint.h"
#include "Design.h"
#include "FlowLayout.h"
#include "Glass.h"
#include "ThemeManager.h"

FloatingBars::FloatingBars(GlassBackdrop *backdrop, QScrollArea *area, QWidget *parent)
    : QWidget(parent), m_backdrop(backdrop), m_area(area)
{
    setProperty("wtFloats", true);          // the layout check: drawn above the page on purpose
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(Design::kWindowMargin, Design::kWindowMargin,
                               Design::kWindowMargin, Design::kBarToPage);
    column->setSpacing(Design::kBarRowGap);
    for (int i = 0; i < 2; ++i) {
        m_rows[i] = new QWidget;
        m_flows[i] = new FlowLayout(m_rows[i], Design::kBarGroupGap, Design::kBarRowGap);
        column->addWidget(m_rows[i]);
    }
    if (m_area) {
        connect(m_area->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
    }
    connect(backdrop, &GlassBackdrop::changed, this, [this] { update(); });
}

GlassGroup *FloatingBars::addGroup(Row row, bool pushRight)
{
    auto *group = new GlassGroup(m_backdrop);
    m_flows[row]->addWidget(group, pushRight ? FlowLayout::PushRight : FlowLayout::None);
    m_groups << group;
    return group;
}

int FloatingBars::heightFor(int width) const
{
    const int inner = width - 2 * Design::kWindowMargin;
    return Design::kWindowMargin + m_flows[0]->heightForWidth(inner) + Design::kBarRowGap
         + m_flows[1]->heightForWidth(inner) + Design::kBarToPage;
}

int FloatingBars::barsBottom() const
{
    return height() - Design::kBarToPage;
}

void FloatingBars::setFloating(bool floating)
{
    if (m_floating == floating)
        return;
    m_floating = floating;
    for (GlassGroup *g : m_groups)
        g->setFloating(floating);
    update();
}

void FloatingBars::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto c = ThemeManager::instance()->colors();
    const bool scrolled = m_floating && m_area && m_area->verticalScrollBar()->value() > 0;

    // SC4: the edge between the bars and the page that has scrolled under them.
    QElapsedTimer clock;
    clock.start();
    if (scrolled) {
        const QRect band(0, 0, width(), height());
        if (Glass::solid()) {
            Artwork::paintPage(p, band, c.window);                       // solid
            const int ow = ClayPaint::outlineWidth();
            p.fillRect(QRect(0, height() - ow, width(), ow), c.border);   // E2's line
        } else {
            const qreal dpr = devicePixelRatioF();
            QImage layer(band.size() * dpr, QImage::Format_ARGB32_Premultiplied);
            layer.setDevicePixelRatio(dpr);
            layer.fill(Qt::transparent);
            {
                QPainter lp(&layer);
                Artwork::paintPage(lp, band, c.window);
                // The page's blurred copy, under the window colour at G3's frost.
                QImage page(band.size() * dpr, QImage::Format_ARGB32_Premultiplied);
                page.setDevicePixelRatio(dpr);
                page.fill(Qt::transparent);
                {
                    QPainter pp(&page);
                    const QImage &copy = m_backdrop->pageCopy();
                    if (!copy.isNull()) {
                        QWidget *frame = m_backdrop->frame();
                        const QPoint here = mapTo(frame, QPoint(0, 0));
                        const QPoint origin = m_backdrop->pageOrigin() - here;
                        const QRect viewport(m_area->viewport()->mapTo(frame, QPoint(0, 0)) - here,
                                             m_area->viewport()->size());
                        const QRect under = band & viewport;
                        pp.setRenderHint(QPainter::SmoothPixmapTransform);
                        const double s = GlassBackdrop::kScale;
                        pp.drawImage(QRectF(under), copy,
                                     QRectF((under.x() - origin.x()) * s, (under.y() - origin.y()) * s,
                                            under.width() * s, under.height() * s));
                    }
                    QColor frost = c.window;
                    frost.setAlphaF(float(Glass::frost()));
                    pp.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                    pp.fillRect(band, frost);
                }
                lp.drawImage(QPoint(0, 0), page);
                // The band fades out over its last 16 px.
                QLinearGradient fade(0, height() - Design::kBarToPage, 0, height());
                fade.setColorAt(0, Qt::black);
                fade.setColorAt(1, Qt::transparent);
                lp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                lp.fillRect(QRect(0, height() - Design::kBarToPage, width(), Design::kBarToPage), fade);
            }
            p.drawImage(QPoint(0, 0), layer);
            Glass::reportFrameCost(clock.nsecsElapsed() / 1e6);      // G11
        }
    }

    // E1: each glass group's shadow, under it (the groups paint themselves on top).
    if (m_floating && !Glass::solid())
        for (GlassGroup *g : m_groups)
            if (g->isVisible())
                Glass::drawShadow(p, QRectF(g->mapTo(this, QPoint(0, 0)), g->size()), g->radius(),
                                  Design::kElevationBar);
}

void FloatingBars::wheelEvent(QWheelEvent *event)
{
    // G14: the wheel over the bars scrolls the page beneath them.
    if (m_floating && m_area) {
        QApplication::sendEvent(m_area->verticalScrollBar(), event);
        return;
    }
    QWidget::wheelEvent(event);
}
