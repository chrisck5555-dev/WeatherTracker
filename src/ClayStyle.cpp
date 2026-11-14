#include "ClayStyle.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QStyleOptionButton>
#include <QStyleOptionTab>
#include <QStyleOptionSlider>
#include <QPolygonF>
#include <QTabBar>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

#include "ClayPaint.h"
#include "Design.h"
#include "Display.h"
#include "Icons.h"
#include "Motion.h"
#include "ThemeManager.h"
#include "Typography.h"

namespace {

// --- Hover and press easing (ST2, ST3) ---------------------------------------------------
// Each button keeps two numbers as properties: "wtHover" (0 = not hovered, 1 = hovered)
// and "wtLift" (1 = raised, 0 = sunk). This filter eases them and repaints the button.
constexpr const char *kHover = "wtHover";
constexpr const char *kLift = "wtLift";

class ButtonMotion : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto *w = qobject_cast<QWidget *>(object);
        if (!w)
            return false;
        switch (event->type()) {
        case QEvent::HoverEnter:
            if (w->isEnabled())
                ease(w, kHover, 1.0, Motion::fadeMs(Design::kTapMs));
            break;
        case QEvent::HoverLeave:
            ease(w, kHover, 0.0, Motion::fadeMs(Design::kTapMs));
            break;
        case QEvent::MouseButtonPress:
            ease(w, kLift, 0.0, 0);                      // sinks at once
            break;
        case QEvent::MouseButtonRelease:
            ease(w, kLift, 1.0, Motion::moveMs(Design::kTapMs));   // rises back
            break;
        case QEvent::EnabledChange:
            if (!w->isEnabled()) {
                ease(w, kHover, 0.0, 0);
                ease(w, kLift, 1.0, 0);
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    static void ease(QWidget *w, const char *name, double to, int ms)
    {
        auto *anim = w->findChild<QVariantAnimation *>(QString::fromLatin1(name),
                                                       Qt::FindDirectChildrenOnly);
        const QVariant now = w->property(name);
        const double from = now.isValid() ? now.toDouble() : (name == kLift ? 1.0 : 0.0);
        if (anim)
            anim->stop();
        if (ms <= 0 || qFuzzyCompare(from, to)) {
            w->setProperty(name, to);
            w->update();
            return;
        }
        if (!anim) {
            anim = new QVariantAnimation(w);
            anim->setObjectName(QString::fromLatin1(name));
            QObject::connect(anim, &QVariantAnimation::valueChanged, w, [w, name](const QVariant &v) {
                w->setProperty(name, v.toDouble());
                w->update();
            });
        }
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setDuration(ms);
        anim->setEasingCurve(QEasingCurve::OutCubic);   // M4: never linear
        anim->start();
    }
};

double hoverOf(const QWidget *w, const QStyleOption *opt)
{
    if (!(opt->state & QStyle::State_Enabled))
        return 0.0;
    const QVariant v = w ? w->property(kHover) : QVariant();
    if (v.isValid())
        return v.toDouble();
    return (opt->state & QStyle::State_MouseOver) ? 1.0 : 0.0;
}

double liftOf(const QWidget *w, const QStyleOption *opt)
{
    if (opt->state & QStyle::State_Sunken)
        return 0.0;
    const QVariant v = w ? w->property(kLift) : QVariant();
    return v.isValid() ? v.toDouble() : 1.0;
}

bool keyboardFocus(const QStyleOption *opt)
{
    return (opt->state & QStyle::State_HasFocus) && (opt->state & QStyle::State_KeyboardFocusChange);
}

Theme::Colors colors() { return ThemeManager::instance()->colors(); }

// --- Button levels (B1) ------------------------------------------------------------------
enum class Kind { Primary, Secondary, Quiet };

Kind kindOf(const QWidget *w)
{
    if (!w)
        return Kind::Secondary;
    const QString kind = w->property("wtKind").toString();
    if (kind == QLatin1String("primary") || w->property("accentFill").toBool())
        return Kind::Primary;
    if (kind == QLatin1String("quiet"))
        return Kind::Quiet;
    return Kind::Secondary;
}

// Fill, overlays and outline of one control shape.
void drawControlShape(QPainter &p, const QRectF &r, double radius, const QColor &fill,
                      const QColor &edge, bool raised, double hover, double lift, bool pressed,
                      bool focus, bool onAccent)
{
    const auto c = colors();
    const int ow = ClayPaint::outlineWidth();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (fill.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(ClayPaint::shape(r, radius));
    }
    if (pressed) {
        ClayPaint::drawOverlay(p, r, radius, Theme::pressOverlay(c, fill));
    } else if (hover > 0.001) {
        QColor h = Theme::hoverOverlay(c, fill);      // as clear on the accent as on a card (D58)
        h.setAlphaF(float(h.alphaF() * hover));
        ClayPaint::drawOverlay(p, r, radius, h);
    }
    if (focus)
        ClayPaint::drawFocusTint(p, r, radius, onAccent);
    if (edge.alpha() > 0) {
        const double half = ow / 2.0;
        p.setPen(QPen(edge, ow));
        p.setBrush(Qt::NoBrush);
        p.drawPath(ClayPaint::shape(r.adjusted(half, half, -half, -half), std::max(0.0, radius - half)));
        if (raised && !ClayPaint::contrastLook())
            ClayPaint::drawRaisedEdge(p, r, radius, edge, lift);
    }
    p.restore();
}

// The label: an optional icon (16 px, 8 px from the text; after it when the widget has the
// "wtIconAfter" property, as on "Next") and the text, centred.
void drawLabel(QPainter &p, const QRect &rect, const QString &text, const QIcon &icon,
               const QFont &font, const QColor &color, bool filledIcon, bool underline,
               const QWidget *w)
{
    const bool iconAfter = w && w->property("wtIconAfter").toBool();
    QFont f = font;
    f.setUnderline(underline);
    const QFontMetrics fm(f);
    const QSize textSize = text.isEmpty() ? QSize() : fm.size(Qt::TextShowMnemonic, text);
    QPixmap pm;
    const qreal dpr = w ? w->devicePixelRatioF() : p.device()->devicePixelRatioF();
    const QString fluent = Icons::nameOf(icon);
    if (!fluent.isEmpty())
        pm = Icons::pixmap(fluent, Design::kIconInText, color, filledIcon, dpr);
    else if (!icon.isNull())
        pm = icon.pixmap(QSize(Design::kIconInText, Design::kIconInText), dpr);
    const int iconW = pm.isNull() ? 0 : Design::kIconInText;
    const int gap = (iconW && !text.isEmpty()) ? Design::kGapButtons : 0;
    const int total = iconW + gap + textSize.width();
    int x = rect.left() + (rect.width() - total) / 2;
    const int iconY = rect.top() + (rect.height() - Design::kIconInText) / 2;
    p.save();
    if (!pm.isNull() && !iconAfter) {
        p.drawPixmap(QPoint(x, iconY), pm);
        x += iconW + gap;
    }
    if (!text.isEmpty()) {
        p.setFont(f);
        p.setPen(color);
        p.drawText(QRect(x, rect.top(), textSize.width() + 1, rect.height()),
                   Qt::AlignVCenter | Qt::AlignHCenter | Qt::TextShowMnemonic, text);
        x += textSize.width() + gap;
    }
    if (!pm.isNull() && iconAfter)
        p.drawPixmap(QPoint(x, iconY), pm);
    p.restore();
}

} // namespace

ClayStyle::ClayStyle() : QProxyStyle("Fusion"), m_motion(new ButtonMotion) {}

ClayStyle::~ClayStyle() { delete m_motion; }

void ClayStyle::polish(QWidget *w)
{
    QProxyStyle::polish(w);
    if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QTabBar *>(w)) {
        w->setAttribute(Qt::WA_Hover);
        w->installEventFilter(m_motion);
    }
}

void ClayStyle::unpolish(QWidget *w)
{
    if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QTabBar *>(w))
        w->removeEventFilter(m_motion);
    QProxyStyle::unpolish(w);
}

void ClayStyle::drawControl(ControlElement element, const QStyleOption *opt, QPainter *p,
                            const QWidget *w) const
{
    if (!ThemeManager::instance()) {
        QProxyStyle::drawControl(element, opt, p, w);
        return;
    }
    const auto c = colors();

    // --- Push buttons --------------------------------------------------------------------
    if (element == CE_PushButtonBevel) {
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
            const Kind kind = kindOf(w);
            const bool enabled = b->state & State_Enabled;
            const bool selected = b->state & State_On;
            const bool pressed = b->state & State_Sunken;
            const bool onAccent = enabled && (kind == Kind::Primary || selected);
            QColor fill, edge;
            if (!enabled) {
                fill = Qt::transparent;                          // ST5: muted, never see-through
                edge = kind == Kind::Quiet ? QColor(Qt::transparent)
                     : ClayPaint::contrastLook() ? c.disabledText : c.border;
            } else if (onAccent) {
                fill = c.accent;
                edge = c.accent;
            } else if (kind == Kind::Quiet) {
                fill = Qt::transparent;                          // E2: no outline
                edge = Qt::transparent;
            } else {
                fill = c.surface;
                edge = c.border;
            }
            const bool raised = enabled && kind != Kind::Quiet;
            drawControlShape(*p, QRectF(b->rect), ClayPaint::controlRadius(), fill, edge, raised,
                             hoverOf(w, opt), liftOf(w, opt), enabled && pressed,
                             keyboardFocus(opt), onAccent);
            return;
        }
    }

    if (element == CE_PushButtonLabel) {
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
            const Kind kind = kindOf(w);
            const bool enabled = b->state & State_Enabled;
            const bool selected = b->state & State_On;
            const bool onAccent = enabled && (kind == Kind::Primary || selected);
            const QColor fg = !enabled ? c.disabledText : onAccent ? c.accentText : c.text;
            QFont f = w ? w->font() : p->font();
            if (onAccent || selected)
                f = semibold(f);
            const bool underline = kind == Kind::Quiet && b->icon.isNull();
            QRect r = b->rect;
            if (kind != Kind::Quiet && !ClayPaint::contrastLook())
                r.translate(0, qRound(1.0 - liftOf(w, opt)));   // follows the button down
            drawLabel(*p, r, b->text, b->icon, f, fg, selected, underline, w);
            return;
        }
    }

    // --- Check boxes and option buttons: hover and focus over the whole control ----------
    if (element == CE_CheckBox || element == CE_RadioButton) {
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
            const QRectF all = QRectF(b->rect);
            const double radius = ClayPaint::controlRadius();
            const double hover = hoverOf(w, opt);
            if (hover > 0.001) {
                QColor h = Theme::hoverOverlay(c);
                h.setAlphaF(float(h.alphaF() * hover));
                ClayPaint::drawOverlay(*p, all, radius, h);
            }
            if (keyboardFocus(opt))
                ClayPaint::drawFocusTint(*p, all, radius);
            QStyleOptionButton o(*b);
            o.state &= ~State_HasFocus;                          // no extra outline
            o.palette.setColor(QPalette::WindowText, (b->state & State_Enabled) ? c.text : c.disabledText);
            // Drawn here rather than by the base style, which would place the box with its
            // own subElementRect and ignore the 8 px inset.
            const bool radio = element == CE_RadioButton;
            QStyleOptionButton box(o);
            box.rect = subElementRect(radio ? SE_RadioButtonIndicator : SE_CheckBoxIndicator, &o, w);
            drawPrimitive(radio ? PE_IndicatorRadioButton : PE_IndicatorCheckBox, &box, p, w);
            QStyleOptionButton label(o);
            label.rect = subElementRect(radio ? SE_RadioButtonContents : SE_CheckBoxContents, &o, w);
            drawControl(radio ? CE_RadioButtonLabel : CE_CheckBoxLabel, &label, p, w);
            return;
        }
    }

    // --- Tabs ----------------------------------------------------------------------------
    if (element == CE_TabBarTabShape) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            const bool selected = tab->state & State_Selected;
            bool pressed = tab->state & State_Sunken;
            if (const auto *bar = qobject_cast<const QTabBar *>(w)) {
                // ReleaseToSelect stores the pressed tab's index here (-1 = none).
                const int down = bar->property("pressedTab").toInt();
                if (down >= 0 && bar->tabAt(tab->rect.center()) == down)
                    pressed = true;
            }
            const QRectF shape = QRectF(tab->rect).adjusted(Design::kGapButtons / 2.0, 4,
                                                            -Design::kGapButtons / 2.0, -4);
            const double hover = (tab->state & State_MouseOver) ? 1.0 : 0.0;
            drawControlShape(*p, shape, ClayPaint::controlRadius(), selected ? c.accent : c.surface,
                             selected ? c.accent : c.border, true, hover, pressed ? 0.0 : 1.0,
                             pressed, keyboardFocus(opt), selected);
            return;
        }
    }

    if (element == CE_TabBarTabLabel) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            const bool selected = tab->state & State_Selected;
            const bool enabled = tab->state & State_Enabled;
            // The open tab stays readable even while the tabs are locked (editing).
            const QColor fg = selected ? c.accentText : !enabled ? c.disabledText : c.text;
            QFont f = w ? w->font() : p->font();
            if (selected)
                f = semibold(f);
            drawLabel(*p, tab->rect, tab->text, tab->icon, f, fg, selected, false, w);
            return;
        }
    }

    QProxyStyle::drawControl(element, opt, p, w);
}

void ClayStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt, QPainter *p,
                              const QWidget *w) const
{
    if (!ThemeManager::instance()) {
        QProxyStyle::drawPrimitive(element, opt, p, w);
        return;
    }
    const auto c = colors();

    if (element == PE_FrameFocusRect) {
        // Buttons, tabs, check boxes and option buttons draw their focus tint themselves.
        if (qobject_cast<const QAbstractButton *>(w) || qobject_cast<const QTabBar *>(w))
            return;
        ClayPaint::drawFocusTint(*p, QRectF(opt->rect), ClayPaint::controlRadius());
        return;
    }

    // Check box: a 20 px box, accent-filled with a check mark when ticked.
    if (element == PE_IndicatorCheckBox || element == PE_IndicatorRadioButton) {
        const bool enabled = opt->state & State_Enabled;
        const bool on = opt->state & State_On;
        const bool radio = element == PE_IndicatorRadioButton;
        const int ow = ClayPaint::outlineWidth();
        const QRectF box = QRectF(opt->rect).adjusted(0.5 * ow, 0.5 * ow, -0.5 * ow, -0.5 * ow);
        const QColor edge = !enabled ? c.disabledText : on ? c.accent : c.inputBorder;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(edge, ow));
        p->setBrush(on && enabled ? c.accent : c.surface);
        if (radio)
            p->drawEllipse(box);
        else
            p->drawPath(ClayPaint::shape(box, ClayPaint::contrastLook() ? Design::kRadiusContrast : 4));
        if (on) {
            const QColor mark = enabled ? c.accentText : c.disabledText;
            if (radio) {
                p->setPen(Qt::NoPen);
                p->setBrush(mark);
                const double d = box.width() * 0.4;
                p->drawEllipse(QRectF(box.center().x() - d / 2, box.center().y() - d / 2, d, d));
            } else {
                const int s = Design::kIconInText;
                const QPixmap pm = Icons::pixmap("checkmark", s, mark, true,
                                                 w ? w->devicePixelRatioF() : 1.0);
                p->drawPixmap(QPointF(opt->rect.center().x() - s / 2.0 + 0.5,
                                      opt->rect.center().y() - s / 2.0 + 0.5), pm);
            }
        }
        p->restore();
        return;
    }

    // The outline around the tab pages (the stylesheet draws it in high contrast).
    if (element == PE_FrameTabWidget && !ClayPaint::contrastLook()) {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(c.border, 1));
        p->setBrush(Qt::NoBrush);
        p->drawPath(ClayPaint::shape(QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5),
                                     Design::kRadiusCard));
        p->restore();
        return;
    }
    QProxyStyle::drawPrimitive(element, opt, p, w);
}

QSize ClayStyle::sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                                  const QWidget *w) const
{
    if (type == CT_PushButton) {
        // `size` is the icon (+4) and text. The label gets 8 px between icon and text, the
        // button 16 px on each side including the outline (S1), and 44 px at least (Z1).
        const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
        int width = size.width();
        int height = size.height();
        if (b) {
            const bool hasIcon = !b->icon.isNull();
            const auto *button = qobject_cast<const QAbstractButton *>(w);
            const bool mayBeSemibold = kindOf(w) == Kind::Primary || (button && button->isCheckable());
            if (mayBeSemibold && w && !b->text.isEmpty()) {
                const QSize semi = QFontMetrics(semibold(w->font())).size(Qt::TextShowMnemonic, b->text);
                width = (hasIcon ? Design::kIconInText + Design::kGapButtons : 0) + semi.width();
                height = std::max(height, semi.height());
            } else if (hasIcon && !b->text.isEmpty()) {
                width += Design::kGapButtons - 4;
            }
        }
        return QSize(std::max(Design::kTarget, width + 2 * Design::kPadCard),
                     std::max(Design::kTarget, height + 2 * Design::kGapButtons));
    }
    if (type == CT_CheckBox || type == CT_RadioButton) {
        // 8 px inside the hover and focus tint on each side (Chris, 2026-11-13: the box
        // sat right at the tint's edge; the tint stays lined up with the column).
        QSize s = QProxyStyle::sizeFromContents(type, opt, size, w);
        return QSize(s.width() + 2 * Design::kGapButtons, std::max(Design::kTarget, s.height()));
    }
    if (type == CT_TabBarTab) {
        QSize s = size;
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            const QFont f = semibold(w ? w->font() : QFont());
            s = QFontMetrics(f).size(Qt::TextShowMnemonic, tab->text);
            if (!tab->icon.isNull())
                s.rwidth() += Design::kIconInText + Design::kGapButtons;
        }
        // The tab's shape is inset 4 px on each side, so neighbouring tabs are 8 apart (S2).
        return QSize(s.width() + 2 * Design::kPadCard + Design::kGapButtons,
                     Design::kTarget + 8);
    }
    return QProxyStyle::sizeFromContents(type, opt, size, w);
}

QRect ClayStyle::subElementRect(SubElement element, const QStyleOption *opt, const QWidget *w) const
{
    switch (element) {
    case SE_CheckBoxIndicator:
    case SE_CheckBoxContents:
    case SE_CheckBoxFocusRect:
    case SE_CheckBoxClickRect:
    case SE_RadioButtonIndicator:
    case SE_RadioButtonContents:
    case SE_RadioButtonFocusRect:
    case SE_RadioButtonClickRect: {
        // The box and its label start 8 px in from the control's edge, where the tint starts.
        QStyleOption inset(*opt);
        inset.rect.adjust(Design::kGapButtons, 0, 0, 0);
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
            QStyleOptionButton ib(*b);
            ib.rect = inset.rect;
            QRect r = QProxyStyle::subElementRect(element, &ib, w);
            if (element == SE_CheckBoxClickRect || element == SE_RadioButtonClickRect)
                r = opt->rect;                    // the whole tinted area stays clickable
            return r;
        }
        return QProxyStyle::subElementRect(element, &inset, w);
    }
    default:
        return QProxyStyle::subElementRect(element, opt, w);
    }
}

int ClayStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *w) const
{
    switch (metric) {
    case PM_ButtonIconSize:
    case PM_SmallIconSize:
        return Design::kIconInText;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return Design::kCheckBox;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return Design::kGapLabel;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0;                                  // the label follows the lift instead
    case PM_TabBarTabShiftHorizontal:
    case PM_TabBarTabShiftVertical:
        return 0;
    case PM_ScrollBarExtent:
    case PM_ScrollBarSliderMin:
        return Display::scrollBarWidth();          // SC2: Windows' own width (17 unless changed)
    default:
        return QProxyStyle::pixelMetric(metric, opt, w);
    }
}

// --- Scroll bars (SC2, D59) -----------------------------------------------------------------
// Laid out and behaving like Windows': Windows' own width, an arrow button at each end, the
// track between, and Qt's standard dragging, track clicks, held arrows and right-click menu.
// Drawn rounded and in the look's colours: the thumb is a pill (R3) in the text box outline
// role (3:1 against the page, C1), darker on hover and when dragged; the arrows' hover and
// press highlight is a rounded square (R2: a control's 8, at most half its side).

QRect ClayStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc,
                                const QWidget *w) const
{
    const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(opt);
    if (cc != CC_ScrollBar || !sb)
        return QProxyStyle::subControlRect(cc, opt, sc, w);
    const bool vertical = sb->orientation == Qt::Vertical;
    const QRect r = sb->rect;
    const int length = vertical ? r.height() : r.width();
    const int thickness = vertical ? r.width() : r.height();
    const int button = std::min(thickness, length / 2);          // square arrow buttons
    const int track = std::max(0, length - 2 * button);
    int thumb = track;
    if (sb->maximum > sb->minimum) {
        const double total = double(sb->maximum - sb->minimum) + sb->pageStep;
        thumb = std::clamp(int(track * sb->pageStep / total), std::min(track, thickness), track);
    }
    const int travel = track - thumb;
    const int at = button + (sb->maximum > sb->minimum
                                 ? int(std::lround(double(travel) * (sb->sliderPosition - sb->minimum)
                                                   / (sb->maximum - sb->minimum)))
                                 : 0);
    auto along = [&](int start, int len) {
        return vertical ? QRect(r.x(), r.y() + start, r.width(), len)
                        : QRect(r.x() + start, r.y(), len, r.height());
    };
    switch (sc) {
    case SC_ScrollBarSubLine: return along(0, button);
    case SC_ScrollBarAddLine: return along(length - button, button);
    case SC_ScrollBarGroove:  return along(button, track);
    case SC_ScrollBarSlider:  return along(at, thumb);
    case SC_ScrollBarSubPage: return along(button, at - button);
    case SC_ScrollBarAddPage: return along(at + thumb, length - button - (at + thumb));
    default:                  return QProxyStyle::subControlRect(cc, opt, sc, w);
    }
}

void ClayStyle::drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p,
                                   const QWidget *w) const
{
    const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(opt);
    if (cc != CC_ScrollBar || !sb || !ThemeManager::instance()) {
        QProxyStyle::drawComplexControl(cc, opt, p, w);
        return;
    }
    const auto c = colors();
    const bool contrast = ClayPaint::contrastLook();
    const bool enabled = sb->state & State_Enabled;
    const bool vertical = sb->orientation == Qt::Vertical;
    constexpr double kInset = 2;                                   // from every side of its part
    p->save();
    p->fillRect(sb->rect, c.surface);                              // the track
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);

    // High contrast: the thumb is in the text colour, so hover and dragging use the accent.
    const QColor thumbRest = contrast ? c.text : c.inputBorder;
    const QColor thumbHover = contrast ? c.accent : Theme::mix(c.inputBorder, c.text, 0.35);
    const QColor thumbDown = contrast ? c.accent : c.text;
    auto stateOf = [&](SubControl part) {
        const bool over = (sb->activeSubControls & part) && (sb->state & State_MouseOver);
        const bool down = (sb->activeSubControls & part) && (sb->state & State_Sunken);
        return std::pair<bool, bool>{over, down};
    };

    // The thumb: a pill, half its width round (4 px corners in high contrast, R2).
    const QRect thumb = subControlRect(cc, sb, SC_ScrollBarSlider, w);
    if (enabled && sb->maximum > sb->minimum && !thumb.isEmpty()) {
        const auto [over, down] = stateOf(SC_ScrollBarSlider);
        const QRectF pill = QRectF(thumb).adjusted(kInset, kInset, -kInset, -kInset);
        const double radius = contrast ? double(Design::kRadiusContrast)
                                       : std::min(pill.width(), pill.height()) / 2.0;
        p->setBrush(down ? thumbDown : over ? thumbHover : thumbRest);
        p->drawPath(ClayPaint::shape(pill, radius));
    }

    // The arrow buttons: a small rounded triangle, on a rounded square while hovered or pressed.
    for (SubControl part : {SC_ScrollBarSubLine, SC_ScrollBarAddLine}) {
        const QRect r = subControlRect(cc, sb, part, w);
        const auto [over, down] = stateOf(part);
        if (enabled && (over || down)) {
            p->setPen(Qt::NoPen);
            p->setBrush(down ? c.mutedText : Theme::mix(c.surface, c.text, 0.12));
            p->drawPath(ClayPaint::shape(QRectF(r).adjusted(kInset, kInset, -kInset, -kInset),
                                         ClayPaint::controlRadius()));
        }
        const QColor glyph = !enabled ? c.disabledText : down ? c.surface : c.mutedText;
        const QPointF m = QRectF(r).center();
        const double s = 3.5;                                      // half the triangle's width
        QPolygonF tri;
        const bool towardStart = part == SC_ScrollBarSubLine;
        if (vertical)
            tri = towardStart ? QPolygonF{{m.x() - s, m.y() + s / 2}, {m.x() + s, m.y() + s / 2}, {m.x(), m.y() - s / 2}}
                              : QPolygonF{{m.x() - s, m.y() - s / 2}, {m.x() + s, m.y() - s / 2}, {m.x(), m.y() + s / 2}};
        else
            tri = towardStart ? QPolygonF{{m.x() + s / 2, m.y() - s}, {m.x() + s / 2, m.y() + s}, {m.x() - s / 2, m.y()}}
                              : QPolygonF{{m.x() - s / 2, m.y() - s}, {m.x() - s / 2, m.y() + s}, {m.x() + s / 2, m.y()}};
        // A same-coloured outline with round joins rounds the triangle's corners.
        p->setPen(QPen(glyph, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(glyph);
        p->drawPolygon(tri);
    }
    p->restore();
}
