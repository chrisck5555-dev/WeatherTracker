#include "Toast.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>

#include "ClayPaint.h"
#include "ThemeManager.h"

Toast::Toast(QWidget *window)
    : QWidget(window)
{
    setAccessibleName("Notice");
    m_text = new QLabel;
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    QFont f = font();
    f.setPointSize(13);
    m_text->setFont(f);

    m_undo = new QPushButton("Undo");
    m_undo->setMinimumSize(110, 44);
    m_undo->setToolTip("Undo (Ctrl+Z)");
    m_close = new QPushButton("×");                 // ×
    m_close->setMinimumSize(44, 44);
    m_close->setMaximumWidth(60);
    m_close->setToolTip("Close this message");
    m_close->setAccessibleName("Close this message");
    for (auto *b : {m_undo, m_close})
        b->setFocusPolicy(Qt::TabFocus);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_undo);
    layout->addWidget(m_close);

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Toast::dismiss);
    connect(m_undo, &QPushButton::clicked, this, &Toast::undo);
    connect(m_close, &QPushButton::clicked, this, &Toast::dismiss);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));

    window->installEventFilter(this);                    // follow the window's size
    hide();
}

void Toast::showMessage(const QString &text, std::function<void()> undo)
{
    m_text->setText(text);
    m_undoAction = std::move(undo);
    m_undo->setVisible(bool(m_undoAction));
    place();
    show();
    raise();
    m_remainingMs = kShowMs;
    m_timer.start(m_remainingMs);
}

bool Toast::undo()
{
    if (!isVisible() || !m_undoAction)
        return false;
    auto action = std::move(m_undoAction);
    m_undoAction = nullptr;
    dismiss();
    action();
    return true;
}

void Toast::dismiss()
{
    m_timer.stop();
    m_undoAction = nullptr;
    hide();
}

void Toast::place()
{
    QWidget *window = parentWidget();
    const int width = qMin(760, window->width() - 48);
    const int height = heightForWidth(width) > 0 ? heightForWidth(width) : sizeHint().height();
    setGeometry((window->width() - width) / 2, window->height() - height - 24, width, height);
}

void Toast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool contrast = ThemeManager::instance()->style() == Theme::Style::HighContrast;
    ClayPaint::drawCard(p, QRectF(rect()).adjusted(1, 1, -1, -1), contrast ? 4 : 16,
                        ThemeManager::instance()->colors().surface);
}

void Toast::enterEvent(QEnterEvent *event)
{
    if (m_timer.isActive()) {                            // she is reading it: hold still
        m_remainingMs = qMax(0, m_timer.remainingTime());
        m_timer.stop();
    }
    QWidget::enterEvent(event);
}

void Toast::leaveEvent(QEvent *event)
{
    if (isVisible())
        m_timer.start(qMax(m_remainingMs, 3000));        // at least 3 more seconds
    QWidget::leaveEvent(event);
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible())
        place();
    return QWidget::eventFilter(watched, event);
}
