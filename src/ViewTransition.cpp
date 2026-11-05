#include "ViewTransition.h"

#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QWidget>

// Draws the two pictures. Always fully covered, so it never needs what is underneath.
class ViewTransition::Cover : public QWidget
{
public:
    explicit Cover(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_TransparentForMouseEvents);   // clicks go to the real view
        setFocusPolicy(Qt::NoFocus);
        setObjectName("viewTransitionCover");
    }

    QPixmap from, to;          // the view before and after the change
    bool forward = true;
    double t = 0;              // 0 = start, 1 = done

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const int w = width();
        if (to.isNull()) {                       // captured, change not shown yet
            p.drawPixmap(0, 0, from);
            return;
        }
        // The page underneath moves 30 % of the way and dims; the page on top slides
        // across with a soft shadow on its leading edge.
        const QPixmap &under = forward ? from : to;
        const QPixmap &over = forward ? to : from;
        const double underPos = forward ? -0.3 * w * t : -0.3 * w * (1 - t);
        const double overPos = forward ? w * (1 - t) : w * t;
        const double dim = 0.14 * (forward ? t : 1 - t);

        p.drawPixmap(QPointF(underPos, 0), under);
        p.fillRect(QRectF(0, 0, overPos, height()), QColor(0, 0, 0, int(255 * dim)));

        QLinearGradient shade(overPos - 14, 0, overPos, 0);
        shade.setColorAt(0, QColor(0, 0, 0, 0));
        shade.setColorAt(1, QColor(0, 0, 0, 45));
        p.fillRect(QRectF(overPos - 14, 0, 14, height()), shade);
        p.drawPixmap(QPointF(overPos, 0), over);
    }
};

ViewTransition::ViewTransition(QWidget *host)
    : QObject(host)
    , m_host(host)
{
    m_cover = new Cover(host);
    m_cover->hide();

    m_anim.setDuration(kDurationMs);
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_cover->t = v.toDouble();
        m_cover->update();
    });
    connect(&m_anim, &QVariantAnimation::finished, this, &ViewTransition::finish);
}

void ViewTransition::setEnabled(bool on)
{
    m_enabled = on;
    if (!on)
        finish();
}

bool ViewTransition::isPlaying() const
{
    return m_cover->isVisible();
}

void ViewTransition::setFloating(const QList<QWidget *> &widgets)
{
    m_floating.clear();
    for (QWidget *w : widgets)
        m_floating << w;
}

QPixmap ViewTransition::grab()
{
    // The window as she sees it (its background included), minus the floating widgets.
    QList<QWidget *> hidden;
    for (const auto &w : m_floating)
        if (w && w->isVisible()) {
            w->hide();
            hidden << w;
        }
    const QPixmap picture = m_host->grab(m_rect);
    for (QWidget *w : hidden)
        w->show();
    return picture;
}

void ViewTransition::capture(QWidget *area)
{
    finish();                                   // interrupt: never stack two animations
    if (!m_enabled || !area || !area->isVisible() || area->size().isEmpty())
        return;
    m_rect = QRect(area->mapTo(m_host, QPoint(0, 0)), area->size());
    m_cover->from = grab();
    m_cover->to = QPixmap();
    m_cover->t = 0;
    m_cover->setGeometry(m_rect);
    m_cover->show();                            // hides the change that follows
    m_cover->raise();
    for (const auto &w : m_floating)
        if (w && w->isVisible())
            w->raise();
    m_captured = true;
}

void ViewTransition::play(Kind kind)
{
    if (!m_captured)
        return;
    // Let the new view lay itself out, then take its picture (with the cover out of it).
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    m_cover->hide();
    m_cover->to = grab();
    m_cover->show();
    m_cover->raise();
    for (const auto &w : m_floating)
        if (w && w->isVisible())
            w->raise();
    m_cover->forward = kind == Kind::Forward;
    m_anim.stop();
    m_anim.start();
}

void ViewTransition::finish()
{
    m_anim.stop();
    m_captured = false;
    m_cover->hide();
    m_cover->from = QPixmap();                  // free the pictures
    m_cover->to = QPixmap();
}
