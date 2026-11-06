#pragma once

#include <QHash>
#include <QObject>
#include <QPointF>
#include <functional>

class QVariantAnimation;
class QWidget;

// Hover for widgets that paint several clickable items themselves (calendar days, weeks,
// months). It watches the mouse over the widget and eases each item's hover amount
// (0 = not hovered, 1 = hovered) in and out over 100 ms (ST2), like ClayStyle's buttons.
// The widget paints Theme::hoverOverlay() scaled by amount(index) on each item.
class HoverFade : public QObject
{
public:
    // `itemAt` returns the item under a point, or -1 for none.
    HoverFade(QWidget *widget, std::function<int(const QPointF &)> itemAt);

    double amount(int index) const { return m_amounts.value(index, 0.0); }
    int current() const { return m_current; }
    void recheck();                     // call when items move under a still mouse

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void setCurrent(int index);

    QWidget *m_widget;
    std::function<int(const QPointF &)> m_itemAt;
    QVariantAnimation *m_anim;
    QHash<int, double> m_amounts;       // items with any hover showing
    QHash<int, double> m_starts;        // their amounts when the last change began
    int m_current = -1;
};
