#include "YearView.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "AppState.h"
#include "Artwork.h"
#include "ClayPaint.h"
#include "KeyboardNav.h"
#include "PeriodMath.h"
#include "Theme.h"
#include "ThemeManager.h"

YearView::YearView(AppState &state, QWidget *parent)
    : QWidget(parent), m_state(state)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        updateAccessibleText();
        update();
    });
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
    setFocusPolicy(Qt::TabFocus);                      // only Tab, not a mouse click
    setAccessibleDescription("Year. The arrow keys choose a month, Page Up and Page Down "
                             "change the year, and Enter opens the month.");
    updateAccessibleText();
}

void YearView::updateAccessibleText()
{
    KeyboardNav::announce(this, m_state.currentDate().toString("MMMM yyyy"));
}

bool YearView::event(QEvent *event)
{
    if (KeyboardNav::claim(event, {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down,
                                   Qt::Key_PageUp, Qt::Key_PageDown}))
        return true;
    return QWidget::event(event);
}

void YearView::keyPressEvent(QKeyEvent *event)
{
    const QDate d = m_state.currentDate();
    switch (event->key()) {
    case Qt::Key_Left:     m_state.setCurrentDate(d.addMonths(-1)); break;
    case Qt::Key_Right:    m_state.setCurrentDate(d.addMonths(1)); break;
    case Qt::Key_Up:       m_state.setCurrentDate(d.addMonths(-4)); break;   // 4 tiles a row
    case Qt::Key_Down:     m_state.setCurrentDate(d.addMonths(4)); break;
    case Qt::Key_PageUp:   m_state.setCurrentDate(d.addYears(-1)); break;
    case Qt::Key_PageDown: m_state.setCurrentDate(d.addYears(1)); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:    emit monthActivated(d); break;
    default:               QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void YearView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateAccessibleText();
    update();
}

void YearView::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

QRectF YearView::tileRect(int i) const
{
    const double w = width() / 4.0, h = (height() - 30) / 3.0;   // 30 px kept for the legend
    return QRectF((i % 4) * w, (i / 4) * h, w, h).adjusted(6, 6, -6, -6);
}

void YearView::mousePressEvent(QMouseEvent *event)
{
    m_pressed = -1;
    if (event->button() != Qt::LeftButton)
        return;
    for (int i = 0; i < 12; ++i)
        if (tileRect(i).contains(event->position()))
            m_pressed = i;
    update();
}

void YearView::mouseReleaseEvent(QMouseEvent *event)
{
    const int i = m_pressed;
    m_pressed = -1;
    update();
    if (event->button() == Qt::LeftButton && i >= 0 && tileRect(i).contains(event->position()))
        emit monthActivated(PeriodMath::withMonth(m_state.currentDate(), i + 1));
}

void YearView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    const bool clay = ThemeManager::instance()->style() == Theme::Style::Clay;
    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), pal.color(QPalette::Window));

    QFont f = font();
    f.setPointSize(16);
    f.setBold(true);
    p.setFont(f);

    const QDate today = QDate::currentDate();
    const int year = m_state.currentDate().year();
    for (int i = 0; i < 12; ++i) {
        const bool down = i == m_pressed;
        const QRectF shape = tileRect(i);
        const QRectF r = down ? shape.translated(0, 2) : shape;
        if (clay) {
            ClayPaint::drawRaised(p, shape, 18, pal.color(QPalette::Base), down);

            // The season's picture inside the tile; the middle stays calm for the month name.
            const QRectF art = r.adjusted(2, 2, -2, -5);
            const QPixmap season = Artwork::seasonTile(i + 1, art.size().toSize(),
                                                       p.device()->devicePixelRatioF());
            if (!season.isNull()) {
                p.save();
                p.setClipPath(ClayPaint::shape(art, 16));
                p.drawPixmap(art.topLeft(), season);
                p.restore();
            }
        } else {
            QColor base = pal.color(QPalette::Base);
            if (down)
                base = base.darker(112);
            p.setPen(pal.color(QPalette::Mid));
            p.setBrush(base);
            p.drawPath(ClayPaint::shape(r, 8));
        }
        p.setPen(pal.color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, QDate(year, i + 1, 1).toString("MMMM"));
        if (year == today.year() && i + 1 == today.month()) {
            p.setPen(QPen(pal.color(QPalette::Highlight), 3));
            p.setBrush(Qt::NoBrush);
            p.drawPath(ClayPaint::shape(r.adjusted(1.5, 1.5, -1.5, -1.5), clay ? 18 : 8));
        }
        if (hasFocus() && i + 1 == m_state.currentDate().month())
            ClayPaint::drawFocusRing(p, r, clay ? 18 : 8);
    }

    // --- Legend ---
    QFont lf = font();
    lf.setPointSize(11);
    p.setFont(lf);
    p.setPen(QPen(pal.color(QPalette::Highlight), 3));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ClayPaint::shape(QRectF(12, height() - 24, 22, 16), clay ? 6 : 0));
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(QRectF(42, height() - 28, 200, 24), Qt::AlignVCenter | Qt::AlignLeft, "Today");
}