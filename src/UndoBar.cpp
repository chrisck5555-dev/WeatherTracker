#include "UndoBar.h"

#include <QAccessible>
#include <QEnterEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <algorithm>

#include "Design.h"
#include "FlowLayout.h"
#include "Icons.h"

// The message: asks for the width of its text on one line, or for `wrapWidth` once the bar
// has decided it wraps beside the buttons (RZ5).
class UndoMessage : public QLabel
{
public:
    using QLabel::QLabel;

    int oneLineWidth() const
    {
        const QMargins m = contentsMargins();
        return fontMetrics().horizontalAdvance(text()) + m.left() + m.right() + 1;
    }
    void setWrapWidth(int px)
    {
        if (px == m_wrap)
            return;
        m_wrap = px;
        updateGeometry();
    }
    QSize sizeHint() const override
    {
        if (m_wrap > 0 && m_wrap < oneLineWidth())
            return QSize(m_wrap, std::max(Design::kTarget, heightForWidth(m_wrap)));
        return QSize(oneLineWidth(), std::max(Design::kTarget, fontMetrics().lineSpacing()));
    }

private:
    int m_wrap = 0;
};

namespace {
constexpr int kMaxWidth = 960;            // plenty for one line of the longest message
}

namespace {

// The undo bar's shadow (E1, elevation 128). A shadow falls outside its shape, so this widget
// sits just under the bar, a little bigger, and lets the mouse through to the page.
class BarShadow : public QWidget
{
public:
    static constexpr int kReach = 72;          // the elevation-128 shadow: blur 48 + drop 24

    BarShadow(UndoBar *bar, QWidget *host) : QWidget(host), m_bar(bar)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setProperty("wtFloats", true);         // the layout check: it floats on purpose
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!m_bar || Glass::solid())
            return;
        QPainter p(this);
        Glass::drawShadow(p, QRectF(m_bar->geometry().translated(-pos())), m_bar->radius(),
                          Design::kElevationDialog);
    }

private:
    QPointer<UndoBar> m_bar;
};

} // namespace

UndoBar::UndoBar(GlassBackdrop *backdrop, QWidget *host)
    : GlassGroup(backdrop, host)
{
    setProperty("wtFloats", true);         // the layout check: it floats on purpose
    setAccessibleName("Notice");
    m_text = new UndoMessage;
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    m_text->setMinimumHeight(Design::kTarget);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_text->setContentsMargins(Design::kGapButtons, 0, Design::kGapButtons, 0);

    m_undo = new QPushButton("Undo");
    m_undo->setIcon(Icons::icon("arrow_undo"));
    m_undo->setToolTip("Put it back the way it was (Ctrl+Z)");
    m_close = new QPushButton("Close");
    m_close->setProperty("wtKind", "quiet");
    m_close->setIcon(Icons::icon("dismiss"));
    m_close->setToolTip("Hide this message. Ctrl+Z still undoes it.");

    // The buttons stay together; on a narrow window they go under the message.
    m_buttons = new QWidget;
    auto *row = new FlowLayout(m_buttons, Design::kGapButtons, Design::kGapButtons);
    row->addWidget(m_undo);
    row->addWidget(m_close);
    flow()->addWidget(m_text);
    flow()->addWidget(m_buttons, FlowLayout::PushRight);

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &UndoBar::dismiss);
    connect(m_undo, &QPushButton::clicked, this, &UndoBar::undo);
    connect(m_close, &QPushButton::clicked, this, &UndoBar::dismiss);
    m_shadow = new BarShadow(this, host);
    m_shadow->hide();
    host->installEventFilter(this);            // follows the window's size on every page
    hide();
}

bool UndoBar::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ZOrderChange:
        syncShadow();
        notifySpace();
        break;
    default:
        break;
    }
    return GlassGroup::event(event);
}

bool UndoBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible())
        place();
    return GlassGroup::eventFilter(watched, event);
}

void UndoBar::syncShadow()
{
    if (!m_shadow)
        return;
    const int r = BarShadow::kReach;
    m_shadow->setGeometry(geometry().adjusted(-r, -r, r, r));
    m_shadow->setVisible(isVisible());
    if (isVisible())
        m_shadow->stackUnder(this);
    m_shadow->update();
}

void UndoBar::showMessage(const QString &text, std::function<void()> undo)
{
    m_text->setText(text);
    m_undoAction = std::move(undo);
    m_undoLabel = m_undoAction ? text : QString();
    m_undo->setVisible(bool(m_undoAction));
    place();
    show();
    raise();
    m_remainingMs = Design::kUndoSeconds * 1000;
    m_timer.start(m_remainingMs);
    // K1: Narrator reads the message without moving the keyboard focus (WCAG 4.1.3).
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QAccessibleAnnouncementEvent event(this, text);
    QAccessible::updateAccessibility(&event);
#endif
}

bool UndoBar::undo()
{
    if (!m_undoAction)
        return false;
    auto action = std::move(m_undoAction);
    forget();
    dismiss();
    action();
    return true;
}

void UndoBar::dismiss()
{
    m_timer.stop();
    hide();
}

void UndoBar::forget()
{
    m_undoAction = nullptr;
    m_undoLabel.clear();
    m_undo->hide();
}

int UndoBar::widthFor(int frameWidth) const
{
    const int room = frameWidth - 2 * Design::kWindowMargin;          // RZ3: never wider
    return std::max(1, std::min({sizeHint().width(), room, kMaxWidth}));
}

int UndoBar::space() const
{
    return isVisible() ? height() + Design::kWindowMargin : 0;
}

void UndoBar::notifySpace()
{
    const int now = space();
    if (now != m_space) {
        m_space = now;
        emit spaceChanged(now);
    }
}

void UndoBar::place()
{
    QWidget *host = parentWidget();
    if (!host)
        return;
    m_text->setWrapWidth(0);                       // measured on one line first
    const int w = widthFor(host->width());
    // RZ5 (D60): when the message and the buttons don't fit on one line, the message wraps to
    // two lines beside the buttons; only when that doesn't fit either do they go under it.
    const QMargins pad = flow()->contentsMargins();
    const int inner = w - pad.left() - pad.right();
    const int buttons = m_buttons->sizeHint().width();
    const int gap = flow()->horizontalGap();
    if (m_text->oneLineWidth() + gap + buttons > inner) {
        const int beside = inner - gap - buttons;
        const int line = m_text->fontMetrics().lineSpacing();
        if (beside > 0 && m_text->heightForWidth(beside) < line * 5 / 2)   // two lines at most
            m_text->setWrapWidth(beside);
    }
    flow()->invalidate();
    const int h = std::max(flow()->heightForWidth(w), Design::barHeight());
    setGeometry((host->width() - w) / 2, host->height() - Design::kWindowMargin - h, w, h);
}

void UndoBar::enterEvent(QEnterEvent *event)
{
    if (m_timer.isActive()) {                     // she is reading it: hold still
        m_remainingMs = std::max(0, m_timer.remainingTime());
        m_timer.stop();
    }
    GlassGroup::enterEvent(event);
}

void UndoBar::leaveEvent(QEvent *event)
{
    if (isVisible())
        m_timer.start(std::max(m_remainingMs, 3000));   // at least 3 more seconds
    GlassGroup::leaveEvent(event);
}
