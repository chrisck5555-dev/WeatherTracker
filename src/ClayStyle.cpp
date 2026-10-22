#include "ClayStyle.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionTab>
#include <QTabBar>
#include <algorithm>

#include "ClayPaint.h"
#include "ThemeManager.h"

namespace {
bool clayActive()
{
    return ThemeManager::instance()
        && ThemeManager::instance()->style() == Theme::Style::Clay;
}
}

ClayStyle::ClayStyle() : QProxyStyle("Fusion") {}

void ClayStyle::drawControl(ControlElement element, const QStyleOption *opt, QPainter *p,
                            const QWidget *w) const
{
    // --- Tabs: selected = pushed in and blue; a pressed tab also sinks ---
    if (element == CE_TabBarTabShape && clayActive()) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            const auto t = ThemeManager::instance()->colors();
            const bool selected = tab->state & State_Selected;
            bool down = tab->state & State_Sunken;
            if (const auto *bar = qobject_cast<const QTabBar *>(w)) {
                // ReleaseToSelect stores the pressed tab's index here (-1 = none).
                const int pressed = bar->property("pressedTab").toInt();
                if (pressed >= 0 && bar->tabAt(tab->rect.center()) == pressed)
                    down = true;
            }
            const QColor base = selected ? t.accent : t.surface;
            ClayPaint::drawRaised(*p, tab->rect.adjusted(2, 4, -2, 0), 14, base,
                                  selected || down);
            return;
        }
    }

    const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
    if (!b || !clayActive()) {
        QProxyStyle::drawControl(element, opt, p, w);
        return;
    }

    const bool pressed = b->state & State_Sunken;
    // Blue fill: a checked button, or one that asks for it with the "accentFill" property.
    const bool accent = (b->state & State_On)
        || (w && w->property("accentFill").toBool());
    const bool enabled = b->state & State_Enabled;
    const bool sunken = pressed || (b->state & State_On);   // only real checked buttons stay in

    if (element == CE_PushButtonBevel) {
        const auto t = ThemeManager::instance()->colors();
        QColor base = accent ? t.accent : t.surface;
        if (!enabled)
            base = t.window;
        const double radius = std::min(18.0, b->rect.height() * 0.35);
        ClayPaint::drawRaised(*p, b->rect, radius, base, sunken);

        // Dotted focus ring only for keyboard focus (Tab), not after a mouse click.
        if ((b->state & State_HasFocus) && (b->state & State_KeyboardFocusChange)) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(QPen(t.text, 2, Qt::DotLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(b->rect).adjusted(1, 1, -1, -3), radius + 2, radius + 2);
            p->restore();
        }
        return;
    }

    if (element == CE_PushButtonLabel) {
        QStyleOptionButton o(*b);
        if (accent && enabled)                       // text on the accent fill
            o.palette.setColor(QPalette::ButtonText,
                               ThemeManager::instance()->colors().accentText);
        o.rect.translate(0, sunken ? 1 : -1);        // follows the body up or down
        QProxyStyle::drawControl(element, &o, p, w);
        return;
    }

    QProxyStyle::drawControl(element, opt, p, w);
}

QSize ClayStyle::sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                                  const QWidget *w) const
{
    QSize s = QProxyStyle::sizeFromContents(type, opt, size, w);
    if (type == CT_PushButton && clayActive())
        s += QSize(20, 8);                           // the stylesheet padding used to do this
    if (type == CT_TabBarTab && clayActive())
        s += QSize(12, 18);                          // taller, a little wider
    return s;
}