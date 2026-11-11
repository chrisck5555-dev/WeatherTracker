#pragma once

#include <QProxyStyle>

// Fusion, plus the program's own buttons, tabs, check boxes and option buttons, in every
// look (design-rules B1, E1, E2, R2, ST1-ST6, F1):
//   - three button levels, set with the "wtKind" property: "primary" (accent fill),
//     "quiet" (no fill or outline; an icon or an underline), or the default secondary
//     (surface fill with an outline). A checked button is "selected": accent fill.
//   - hover is an 8% tint toward the text colour, easing in over 100 ms; pressed is a 10%
//     tint, and a raised button sinks at once and rises back over 100 ms
//   - keyboard focus is a tint of the accent over the whole control
//   - disabled is muted colours, no hover and no raised edge (never see-through)
//   - Fluent icons (Icons.h) are drawn in the label's colour, Filled when selected
//   - scroll bars are Windows 10's (SC2): 17 px, an arrow button at each end and a plain
//     thumb, in the look's colours
class ClayStyle : public QProxyStyle
{
public:
    ClayStyle();
    ~ClayStyle() override;

    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option,
                    const QWidget *widget) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                         SubControl sub, const QWidget *widget) const override;

private:
    QObject *m_motion;                          // eases hover and press (ButtonMotion)
};
