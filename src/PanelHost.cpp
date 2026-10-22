#include "PanelHost.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>

#include "ClayPaint.h"

namespace {

// The card itself: a flat rounded panel with a thin outline.
class CardFrame : public QWidget
{
public:
    explicit CardFrame(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_NoMousePropagation);       // clicks on the card never reach the dim area
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        ClayPaint::drawCard(p, QRectF(rect()).adjusted(1, 1, -1, -1), 18, palette().color(QPalette::Base));
    }
};

constexpr int kEdge = 30;          // least gap between the card and the window edge

} // namespace

PanelHost::PanelHost(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground, false);
    setFocusPolicy(Qt::NoFocus);
    m_card = new CardFrame(this);
    auto *layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(22, 20, 22, 20);

    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &PanelHost::closePanel);

    parent->installEventFilter(this);
    setGeometry(parent->rect());
    hide();
}

void PanelHost::setUnderlying(QWidget *widget)
{
    m_underlying = widget;
}

void PanelHost::showPanel(QWidget *content, const QSize &preferred)
{
    if (m_content)
        closePanel();

    m_previousFocus = QApplication::focusWidget();
    m_content = content;
    m_preferred = preferred;
    m_card->layout()->addWidget(content);
    content->show();

    if (m_underlying)
        m_underlying->setEnabled(false);
    setGeometry(parentWidget()->rect());
    show();
    raise();
    place();
    content->setFocus();
    emit opened();
}

void PanelHost::closePanel()
{
    if (!m_content)
        return;
    QWidget *content = m_content;
    m_content = nullptr;                    // isOpen() is false from here on
    content->hide();
    content->deleteLater();                 // may be running inside the content's own signal

    if (m_underlying)
        m_underlying->setEnabled(true);
    hide();
    if (m_previousFocus)
        m_previousFocus->setFocus();
    emit closed();
}

void PanelHost::setAvoid(const QRect &area)
{
    m_avoid = area;
    place();
}

void PanelHost::place()
{
    if (!m_content)
        return;
    QRect area = rect();
    if (!m_avoid.isNull()) {                          // e.g. the tour card: use the side without it
        if (m_avoid.center().x() > width() / 2)
            area.setRight(m_avoid.left() - 16);
        else
            area.setLeft(m_avoid.right() + 16);
    }
    const int w = qBound(300, m_preferred.width(), qMax(300, area.width() - 2 * kEdge));
    const int h = qBound(240, m_preferred.height(), qMax(240, area.height() - 2 * kEdge));
    m_card->setGeometry(area.left() + (area.width() - w) / 2, area.top() + (area.height() - h) / 2, w, h);
}

void PanelHost::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 140));
}

void PanelHost::mousePressEvent(QMouseEvent *event)
{
    m_pressedOutside = !m_card->geometry().contains(event->pos());
    event->accept();
}

void PanelHost::mouseReleaseEvent(QMouseEvent *event)
{
    // Close on release, and only if the press was also outside: no click-through.
    const bool outside = !m_card->geometry().contains(event->pos());
    const bool close = m_pressedOutside && outside;
    m_pressedOutside = false;
    event->accept();
    if (close)
        closePanel();
}

bool PanelHost::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        place();
    }
    return QWidget::eventFilter(watched, event);
}
