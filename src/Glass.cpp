#include "Glass.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPainterPath>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWidget>
#include <cmath>

#include "Artwork.h"
#include "BoxBlur.h"
#include "ClayPaint.h"
#include "Design.h"
#include "Display.h"
#include "FlowLayout.h"
#include "ThemeManager.h"

namespace Glass {

namespace {

struct SystemState {
    bool transparencyOff = false;
    bool batterySaver = false;
    bool read = false;
    bool slow = false;               // set for the rest of the session (G11)
    int slowFrames = 0;
    double frameMs = 0;              // glass drawn so far in this frame
    bool frameOpen = false;
};

SystemState &state()
{
    static SystemState s;
    if (!s.read) {
        s.read = true;
        s.transparencyOff = Display::transparencyOff();
        s.batterySaver = Display::batterySaverOn();
    }
    return s;
}

bool darkLook(const Theme::Colors &c) { return c.text.lightnessF() > c.window.lightnessF(); }

struct ShadowLayer { double dy, blur, alpha; };

// E1: the higher it floats, the larger and softer its shadow. Values follow the mock-ups
// for the bars (16); the panel and dialog values grow from them.
QList<ShadowLayer> layersFor(int elevation)
{
    if (elevation >= Design::kElevationDialog)
        return {{24, 48, 0.22}, {2, 8, 0.10}};
    if (elevation >= Design::kElevationPanel)
        return {{12, 28, 0.18}, {2, 6, 0.10}};
    return {{6, 18, 0.16}, {1, 3, 0.10}};
}

} // namespace

bool solid()
{
    const SystemState &s = state();
    return ClayPaint::contrastLook() || s.transparencyOff || s.batterySaver || s.slow;
}

void refreshSystem()
{
    SystemState &s = state();
    s.transparencyOff = Display::transparencyOff();
    s.batterySaver = Display::batterySaverOn();
}

double frost()
{
    const auto *themes = ThemeManager::instance();
    return themes && darkLook(themes->colors()) ? Design::kFrostDark : Design::kFrostLight;
}

void reportFrameCost(double ms)
{
    SystemState &s = state();
    if (s.slow)
        return;
    // Everything glass draws in one frame is added up; the frame ends when the event loop
    // gets back control.
    s.frameMs += ms;
    if (s.frameOpen)
        return;
    s.frameOpen = true;
    QTimer::singleShot(0, [] {
        SystemState &st = state();
        st.slowFrames = st.frameMs > Design::kGlassBudgetMs ? st.slowFrames + 1 : 0;
        st.frameMs = 0;
        st.frameOpen = false;
        if (st.slowFrames >= 3)
            st.slow = true;
    });
}

void drawShadow(QPainter &p, const QRectF &shape, double radius, int elevation)
{
    if (ClayPaint::contrastLook() || shape.isEmpty())
        return;
    const auto c = ThemeManager::instance()->colors();
    const auto layers = layersFor(elevation);
    double margin = 0;
    for (const ShadowLayer &l : layers)
        margin = std::max(margin, l.blur + l.dy);
    const QSize size = shape.size().toSize();
    const QString key = QString("%1x%2|%3|%4|%5").arg(size.width()).arg(size.height())
                            .arg(radius).arg(elevation).arg(c.shadow.rgb());
    static QHash<QString, QImage> cache;
    QImage img = cache.value(key);
    if (img.isNull()) {
        // Drawn and blurred at half size, which a soft shadow can't tell apart.
        constexpr double half = 0.5;
        const QSizeF full(size.width() + 2 * margin, size.height() + 2 * margin);
        img = QImage((full * half).toSize() + QSize(1, 1), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        for (const ShadowLayer &l : layers) {
            QImage layer(img.size(), QImage::Format_ARGB32_Premultiplied);
            layer.fill(Qt::transparent);
            {
                QPainter lp(&layer);
                lp.setRenderHint(QPainter::Antialiasing);
                lp.scale(half, half);
                QColor col = c.shadow;
                col.setAlphaF(float(l.alpha));
                lp.setPen(Qt::NoPen);
                lp.setBrush(col);
                lp.drawPath(ClayPaint::shape(QRectF(margin, margin + l.dy, size.width(), size.height()), radius));
            }
            BoxBlur::blur(layer, BoxBlur::passRadius(int(l.blur), half));
            QPainter ip(&img);
            ip.drawImage(0, 0, layer);
        }
        if (cache.size() > 48)
            cache.clear();
        cache.insert(key, img);
    }
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    // Only outside the shape: the glass itself is see-through.
    QPainterPath outside;
    outside.addRect(shape.adjusted(-margin, -margin, margin, margin));
    outside = outside.subtracted(ClayPaint::shape(shape, radius));
    p.setClipPath(outside, Qt::IntersectClip);
    p.drawImage(QRectF(shape.x() - margin, shape.y() - margin, size.width() + 2 * margin,
                       size.height() + 2 * margin), img);
    p.restore();
}

} // namespace Glass

// ------------------------------------------------------------------------------------------

GlassBackdrop::GlassBackdrop(QWidget *frame, QScrollArea *area)
    : QObject(frame), m_frame(frame), m_area(area)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);            // RZ4, G11: once things have stopped changing
    connect(&m_settle, &QTimer::timeout, this, [this] {
        if (m_pageStale && m_area && (m_area->verticalScrollBar()->value() > 0 || m_pageUnderGlass)) {
            makePageCopy();
            emit changed();
        }
        if (m_backgroundStale) {
            makeBackgroundCopy();
            emit changed();
        }
    });
}

void GlassBackdrop::invalidate()
{
    m_pageStale = true;
    m_settle.start();
}

void GlassBackdrop::setPageUnderGlass(bool on)
{
    m_pageUnderGlass = on;
}

void GlassBackdrop::invalidateBackground()
{
    m_backgroundStale = true;
    m_pageStale = true;
    m_settle.start();
}

void GlassBackdrop::makeBackgroundCopy()
{
    m_backgroundStale = false;
    if (!m_frame)
        return;
    const QSize size = m_frame->size();
    m_background = QImage(QSize(int(std::ceil(size.width() * kScale)), int(std::ceil(size.height() * kScale))),
                          QImage::Format_ARGB32_Premultiplied);
    m_background.fill(ThemeManager::instance()->colors().window);
    QPainter p(&m_background);
    p.scale(kScale, kScale);
    Artwork::paintPage(p, QRectF(QPointF(0, 0), size), ThemeManager::instance()->colors().window);
    p.end();
    BoxBlur::blur(m_background, BoxBlur::passRadius(Design::kBlurRadius, kScale));
}

void GlassBackdrop::makePageCopy()
{
    m_pageStale = false;
    QWidget *content = m_area ? m_area->widget() : nullptr;
    if (!content || content->size().isEmpty()) {
        m_page = QImage();
        return;
    }
    const QSize cs = content->size();
    m_page = QImage(QSize(int(std::ceil(cs.width() * kScale)), int(std::ceil(cs.height() * kScale))),
                    QImage::Format_ARGB32_Premultiplied);
    m_page.fill(Qt::transparent);
    QPainter p(&m_page);
    p.scale(kScale, kScale);
    content->render(&p, QPoint(), QRegion(), QWidget::DrawChildren);
    p.end();
    BoxBlur::blur(m_page, BoxBlur::passRadius(Design::kBlurRadius, kScale));
}

const QImage &GlassBackdrop::pageCopy()
{
    // A stale copy is still used while things settle; only a missing one is made at once.
    if (m_page.isNull() || (m_pageStale && !m_settle.isActive()))
        makePageCopy();
    return m_page;
}

QPoint GlassBackdrop::pageOrigin() const
{
    QWidget *content = m_area ? m_area->widget() : nullptr;
    return content && m_frame ? content->mapTo(m_frame, QPoint(0, 0)) : QPoint();
}

void GlassBackdrop::paint(QPainter &p, const QRectF &target, const QRect &frameRect)
{
    if (!m_frame || frameRect.isEmpty())
        return;
    if (m_background.isNull() || m_background.size() != QSize(int(std::ceil(m_frame->width() * kScale)),
                                                              int(std::ceil(m_frame->height() * kScale))))
        makeBackgroundCopy();
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF src(frameRect.x() * kScale, frameRect.y() * kScale,
                     frameRect.width() * kScale, frameRect.height() * kScale);
    p.drawImage(target, m_background, src);

    // The page, only where it can be seen in its scroll area.
    if (m_area && (m_area->verticalScrollBar()->value() > 0 || m_pageUnderGlass)) {
        const QRect viewport(m_area->viewport()->mapTo(m_frame, QPoint(0, 0)), m_area->viewport()->size());
        const QRect under = frameRect & viewport;
        if (!under.isEmpty()) {
            const QImage &page = pageCopy();
            if (!page.isNull()) {
                const QPoint origin = pageOrigin();
                const double sx = target.width() / frameRect.width();
                const double sy = target.height() / frameRect.height();
                const QRectF dst(target.x() + (under.x() - frameRect.x()) * sx,
                                 target.y() + (under.y() - frameRect.y()) * sy,
                                 under.width() * sx, under.height() * sy);
                const QRectF from((under.x() - origin.x()) * kScale, (under.y() - origin.y()) * kScale,
                                  under.width() * kScale, under.height() * kScale);
                p.drawImage(dst, page, from);
            }
        }
    }
    p.restore();
}

// ------------------------------------------------------------------------------------------

void Glass::paintShape(QPainter &p, QWidget *w, const QRectF &shape, double radius,
                       GlassBackdrop *backdrop, bool floating)
{
    QElapsedTimer clock;
    clock.start();
    const auto c = ThemeManager::instance()->colors();
    const bool contrast = ClayPaint::contrastLook();
    const QPainterPath path = ClayPaint::shape(shape, radius);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (!solid() && floating && backdrop) {
        p.setClipPath(path, Qt::IntersectClip);
        // Through the window, so it works for glass that isn't inside the frame (the undo bar).
        QWidget *top = w->window();
        QWidget *frame = backdrop->frame() ? backdrop->frame() : top;
        const QPoint at = w->mapTo(top, shape.topLeft().toPoint()) - frame->mapTo(top, QPoint());
        const QRect inFrame(at, shape.size().toSize());
        backdrop->paint(p, shape, inFrame);
        QColor glass = c.raised;
        glass.setAlphaF(float(frost()));                // G3
        p.fillRect(shape, glass);
        p.setClipping(false);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(c.raised);                           // G9: the glass colour at 100%
        p.drawPath(path);
    }
    // G10: a 1 px edge in the highlight role; E2: 3 px in high contrast.
    const double ow = contrast ? Design::kOutlineContrast : Design::kOutline;
    p.setPen(QPen(contrast ? c.border : c.highlight, ow));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ClayPaint::shape(shape.adjusted(ow / 2, ow / 2, -ow / 2, -ow / 2),
                                std::max(0.0, radius - ow / 2)));
    p.restore();
    if (floating && !solid())
        reportFrameCost(clock.nsecsElapsed() / 1e6);
}

GlassGroup::GlassGroup(GlassBackdrop *backdrop, QWidget *parent)
    : QWidget(parent), m_backdrop(backdrop)
{
    m_flow = new FlowLayout(this, Design::kGapButtons, Design::kGapButtons);
    m_flow->setContentsMargins(Design::kBarInnerPad, Design::kBarInnerPad,
                               Design::kBarInnerPad, Design::kBarInnerPad);
    // Its height comes from heightForWidth: a Fixed policy would cap it at one line.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void GlassGroup::setFloating(bool floating)
{
    if (m_floating == floating)
        return;
    m_floating = floating;
    update();
}

double GlassGroup::radius() const { return ClayPaint::cardRadius(); }   // R6: 16, or 4

void GlassGroup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    Glass::paintShape(p, this, QRectF(rect()), radius(), m_backdrop, m_floating);
}
