#include "ClayStyle.h"

#include <QPainter>
#include <QPushButton>
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
            const QRectF body = QRectF(tab->rect).adjusted(2, 4, -2, 0);
            const bool sunken = selected || down;
            ClayPaint::drawRaised(*p, body, 14, base, sunken);
            if ((tab->state & State_HasFocus) && (tab->state & State_KeyboardFocusChange)) {
                // The same outline as the buttons: dashed, text colour, just outside the tab.
                // (drawRaised draws the tab this far inside `body`.)
                const QRectF drawn = sunken ? body.adjusted(2, 4, -2, -3) : body.adjusted(2, 2, -2, -5);
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(ClayPaint::focusPen());
                p->setBrush(Qt::NoBrush);
                p->drawPath(ClayPaint::shape(drawn.adjusted(-1, -1, 1, 1), 16));
                p->restore();
            }
            return;
        }
    }

    if (element == CE_TabBarTabLabel && ThemeManager::instance()) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            const auto t = ThemeManager::instance()->colors();
            const bool selected = tab->state & State_Selected;
            QStyleOptionTab o(*tab);
            // Fusion would draw its own square outline with the label; the tab shape has ours.
            o.state &= ~State_HasFocus;
            if (clayActive() && selected) {
                // The selected tab is blue: its label uses the text colour made for blue.
                o.palette.setColor(QPalette::WindowText, t.accentText);
                o.palette.setColor(QPalette::ButtonText, t.accentText);
            }
            QProxyStyle::drawControl(element, &o, p, w);
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

        // The focus outline only for keyboard focus (Tab), not after a mouse click.
        if ((b->state & State_HasFocus) && (b->state & State_KeyboardFocusChange)) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(ClayPaint::focusPen());
            p->setBrush(Qt::NoBrush);
            p->drawPath(ClayPaint::shape(QRectF(b->rect).adjusted(1, 1, -1, -3), radius + 2));
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

void ClayStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt, QPainter *p,
                              const QWidget *w) const
{
    // Every other keyboard focus outline (high-contrast buttons and tabs, check boxes...)
    // looks the same as the one on clay buttons and calendar days.
    if (element == PE_FrameFocusRect) {
        const auto *bar = qobject_cast<const QTabBar *>(w);
        if (bar && clayActive())
            return;                                     // clay tabs draw their own, on the shape
        const bool contrast = !clayActive();
        QPen pen = ClayPaint::focusPen();
        QRectF ring = QRectF(opt->rect).adjusted(1, 1, -1, -1);
        if (bar) {
            // High contrast: the stylesheet asks for the outline around the text only, which
            // would cross the letters. It goes around the whole tab instead: inside the
            // 4px right margin and 3px border from the stylesheet, with a 3px gap.
            // The focused tab is the selected (blue) one.
            const int index = bar->currentIndex();
            if (index < 0)
                return;
            ring = QRectF(bar->tabRect(index)).adjusted(6, 6, -10, -6);
            pen.setColor(ThemeManager::instance()->colors().accentText);
        }
        const auto *button = qobject_cast<const QPushButton *>(w);
        if (button && contrast) {
            // High-contrast buttons: the same, around the whole button instead of the text,
            // 2px inside its 3px border. A chosen (blue) button gets the text colour for blue.
            ring = QRectF(button->rect()).adjusted(5, 5, -5, -5);
            if (button->isChecked())
                pen.setColor(ThemeManager::instance()->colors().accentText);
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(ClayPaint::shape(ring, contrast ? ((bar || button) ? 2 : 4) : 8));
        p->restore();
        return;
    }
    // The outline around the tab pages (the stylesheet draws it in high contrast).
    if (element == PE_FrameTabWidget && clayActive()) {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(ThemeManager::instance()->colors().border, 1));
        p->setBrush(Qt::NoBrush);
        p->drawPath(ClayPaint::shape(QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5), 16));
        p->restore();
        return;
    }
    QProxyStyle::drawPrimitive(element, opt, p, w);
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