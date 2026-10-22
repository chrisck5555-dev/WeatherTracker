#include "ClayPaint.h"

#include <QLinearGradient>
#include <QPainter>

#include "ThemeManager.h"

namespace ClayPaint {

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
        p.drawRoundedRect(body.translated(0, 3), radius, radius);
    }

    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0, base.lighter(sunken ? 100 : 106));
    fill.setColorAt(1, base.darker(sunken ? 112 : 104));
    p.setPen(QPen(t.border, 1));
    p.setBrush(fill);
    p.drawRoundedRect(body, radius, radius);

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
    p.drawRoundedRect(body.adjusted(1.5, 1.5, -1.5, -1.5), radius - 1.5, radius - 1.5);

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
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p.restore();
}

} // namespace ClayPaint