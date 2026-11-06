#include "HoverFade.h"

#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QWidget>

#include "Design.h"
#include "Motion.h"

HoverFade::HoverFade(QWidget *widget, std::function<int(const QPointF &)> itemAt)
    : QObject(widget), m_widget(widget), m_itemAt(std::move(itemAt))
{
    widget->setMouseTracking(true);     // move events without a button held
    widget->installEventFilter(this);
    m_anim = new QVariantAnimation(this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);   // M4: never linear
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const double t = v.toDouble();
        for (auto it = m_starts.cbegin(); it != m_starts.cend(); ++it) {
            const double from = it.value();
            m_amounts[it.key()] = it.key() == m_current ? from + (1.0 - from) * t : from * (1.0 - t);
        }
        m_widget->update();
    });
    connect(m_anim, &QVariantAnimation::finished, this, [this] {
        m_amounts.removeIf([this](QHash<int, double>::iterator it) { return it.key() != m_current; });
        m_starts.clear();
    });
}

void HoverFade::recheck()
{
    if (!m_widget->isVisible() || !m_widget->underMouse())
        setCurrent(-1);
    else
        setCurrent(m_itemAt(m_widget->mapFromGlobal(QCursor::pos())));
}

bool HoverFade::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_widget) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::Enter:
            if (event->type() == QEvent::MouseMove)
                setCurrent(m_itemAt(static_cast<QMouseEvent *>(event)->position()));
            else
                recheck();
            break;
        case QEvent::Leave:
        case QEvent::Hide:
            setCurrent(-1);
            break;
        case QEvent::EnabledChange:
            if (!m_widget->isEnabled())
                setCurrent(-1);         // ST5: no hover on disabled things
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(object, event);
}

void HoverFade::setCurrent(int index)
{
    if (!m_widget->isEnabled())
        index = -1;
    if (index == m_current)
        return;
    m_current = index;
    // Start every item from where it is now, so a quick move never snaps (M5).
    m_anim->stop();
    m_starts = m_amounts;
    if (index >= 0 && !m_starts.contains(index))
        m_starts.insert(index, 0.0);
    const int ms = Motion::fadeMs(Design::kTapMs);
    if (ms <= 0) {
        m_amounts.clear();
        m_starts.clear();
        if (index >= 0)
            m_amounts.insert(index, 1.0);
        m_widget->update();
        return;
    }
    m_amounts = m_starts;
    m_anim->setDuration(ms);
    m_anim->start();
}
