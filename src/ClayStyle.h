#pragma once

#include <QProxyStyle>

// Fusion, plus puffy push buttons and tabs when the Clay theme is active.
class ClayStyle : public QProxyStyle
{
public:
    ClayStyle();

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
};