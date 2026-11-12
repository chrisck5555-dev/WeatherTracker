#include "ClayPaint.h"

#include <QLinearGradient>
#include <QPainter>
#include <QTransform>
#include <cmath>

#include "Design.h"
#include "ThemeManager.h"

namespace ClayPaint {

namespace {
constexpr double kPi = 3.14159265358979323846;
double rad(double degrees) { return degrees * kPi / 180.0; }
}

// After figma-squircle (MIT): each corner is a cubic curve, a short circular arc, and a
// cubic curve. Built for the top-right corner with the corner at (0,0), the top edge
// coming in from the left and the right edge going down, then turned for the others.
QPainterPath continuousRect(const QRectF &r, double radius, double smoothing)
{
    QPainterPath path;
    const double budget = std::min(r.width(), r.height()) / 2.0;
    radius = std::min(radius, budget);
    if (radius <= 0.01) {
        path.addRect(r);
        return path;
    }
    double p = (1 + smoothing) * radius;
    if (p > budget) {                                   // not enough room: smooth less
        smoothing = std::max(0.0, std::min(smoothing, budget / radius - 1));
        p = std::min(p, budget);
    }
    const double arcMeasure = 90 * (1 - smoothing);
    const double arcLength = std::sin(rad(arcMeasure / 2)) * radius * std::sqrt(2.0);
    const double alpha = (90 - arcMeasure) / 2;
    const double p3ToP4 = radius * std::tan(rad(alpha / 2));
    const double beta = 45 * smoothing;
    const double c = p3ToP4 * std::cos(rad(beta));
    const double d = c * std::tan(rad(beta));
    const double b = (p - arcLength - c - d) / 3;
    const double a = 2 * b;

    // One corner, local coordinates.
    QPainterPath corner;
    const QPointF start(-p, 0);
    corner.moveTo(start);
    const QPointF arcStart = start + QPointF(a + b + c, d);
    corner.cubicTo(start + QPointF(a, 0), start + QPointF(a + b, 0), arcStart);
    const QPointF centre(-radius, radius);
    const double startAngle = std::atan2(-(arcStart.y() - centre.y()), arcStart.x() - centre.x())
                              * 180.0 / kPi;
    corner.arcTo(QRectF(centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius),
                 startAngle, -arcMeasure);
    const QPointF arcEnd = corner.currentPosition();
    corner.cubicTo(arcEnd + QPointF(d, c), arcEnd + QPointF(d, b + c), QPointF(0, p));

    // Top right, bottom right, bottom left, top left: turned 0, 90, 180, 270 degrees.
    const QPointF corners[] = {r.topRight(), r.bottomRight(), r.bottomLeft(), r.topLeft()};
    for (int i = 0; i < 4; ++i) {
        QTransform t;
        t.translate(corners[i].x(), corners[i].y());
        t.rotate(90 * i);
        const QPainterPath piece = t.map(corner);
        if (i == 0)
            path.addPath(piece);
        else
            path.connectPath(piece);                    // a straight edge, then the corner
    }
    path.closeSubpath();
    return path;
}

QPainterPath shape(const QRectF &r, double radius)
{
    const bool clay = !ThemeManager::instance()
                      || ThemeManager::instance()->style() == Theme::Style::Clay;
    if (clay)
        return continuousRect(r, radius);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    return path;
}

void drawRaised(QPainter &p, const QRectF &r, double radius, const QColor &base, bool sunken)
{
    const auto t = ThemeManager::instance()->colors();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    QRectF body = r.adjusted(2, 2, -2, -5);          // bottom margin holds the shadow
    if (sunken)
        body.translate(0, 2);

    if (!sunken) {                                   // soft drop shadow
        QColor sh = t.shadow;
        sh.setAlpha(110);
        p.setPen(Qt::NoPen);
        p.setBrush(sh);
        p.drawPath(shape(body.translated(0, 3), radius));
    }

    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0, base.lighter(sunken ? 100 : 106));
    fill.setColorAt(1, base.darker(sunken ? 112 : 104));
    p.setPen(QPen(t.border, 1));
    p.setBrush(fill);
    p.drawPath(shape(body, radius));

    // Inner edge: bright glint along the top, darker rim along the bottom
    QColor glint = t.highlight;
    glint.setAlpha(230);
    QColor clear = glint;
    clear.setAlpha(0);
    QColor rim = t.shadow;
    rim.setAlpha(80);
    QColor rimClear = rim;
    rimClear.setAlpha(0);
    QLinearGradient edge(body.topLeft(), body.bottomLeft());
    edge.setColorAt(0.0, glint);
    edge.setColorAt(0.4, clear);
    edge.setColorAt(0.65, rimClear);
    edge.setColorAt(1.0, rim);
    p.setPen(QPen(QBrush(edge), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape(body.adjusted(1.5, 1.5, -1.5, -1.5), radius - 1.5));

    p.restore();
}

void drawCard(QPainter &p, const QRectF &r, double radius, const QColor &fill)
{
    const auto t = ThemeManager::instance()->colors();
    // Clay borders are pale, so darken them toward the text color; contrast looks keep theirs.
    QColor edge = t.border;
    if (ThemeManager::instance()->style() == Theme::Style::Clay) {
        edge = QColor::fromRgbF(0.6 * t.border.redF() + 0.4 * t.text.redF(),
                                0.6 * t.border.greenF() + 0.4 * t.text.greenF(),
                                0.6 * t.border.blueF() + 0.4 * t.text.blueF());
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(edge, 1));
    p.setBrush(fill);
    p.drawPath(shape(r.adjusted(0.5, 0.5, -0.5, -0.5), radius));
    p.restore();
}

double controlRadius() { return contrastLook() ? Design::kRadiusContrast : Design::kRadiusControl; }
double cardRadius() { return contrastLook() ? Design::kRadiusContrast : Design::kRadiusCard; }

bool contrastLook()
{
    return ThemeManager::instance()
        && ThemeManager::instance()->style() == Theme::Style::HighContrast;
}

int outlineWidth() { return contrastLook() ? Design::kOutlineContrast : Design::kOutline; }

void drawOverlay(QPainter &p, const QRectF &r, double radius, const QColor &overlay)
{
    if (overlay.alpha() == 0)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(overlay);
    p.drawPath(shape(r, radius));
    p.restore();
}

void drawRaisedEdge(QPainter &p, const QRectF &shapeRect, double radius, const QColor &edge,
                    double lift)
{
    const auto *themes = ThemeManager::instance();
    if (!themes || lift <= 0.01)
        return;
    const auto c = themes->colors();
    const bool dark = c.text.lightnessF() > c.window.lightnessF();
    // Toward the text colour: darker in light looks, lighter in dark ones.
    const QColor shade = Theme::mix(edge, c.text, 0.28 * lift);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (dark)
        p.setClipRect(QRectF(shapeRect.left() - 1, shapeRect.top() - 1,
                             shapeRect.width() + 2, radius + 1));
    else
        p.setClipRect(QRectF(shapeRect.left() - 1, shapeRect.bottom() - radius,
                             shapeRect.width() + 2, radius + 2));
    p.setPen(QPen(shade, 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape(shapeRect.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5));
    p.restore();
}

void drawHover(QPainter &p, const QRectF &r, double radius, double amount)
{
    const auto *themes = ThemeManager::instance();
    if (!themes || amount <= 0.001)
        return;
    QColor h = Theme::hoverOverlay(themes->colors());
    h.setAlphaF(float(h.alphaF() * qMin(amount, 1.0)));
    drawOverlay(p, r, radius, h);
}

void drawFocusTint(QPainter &p, const QRectF &r, double radius, bool onAccent)
{
    const auto *themes = ThemeManager::instance();
    if (!themes)
        return;
    drawOverlay(p, r, radius, Theme::focusOverlay(themes->style(), themes->colors(), onAccent));
}

} // namespace ClayPaint
