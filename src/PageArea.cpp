#include "PageArea.h"

#include <QApplication>
#include <QScrollBar>
#include <QVBoxLayout>

#include "Design.h"

PageArea::PageArea(QWidget *parent)
    : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);            // the page's controls take the focus, not the page
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // SC1
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);     // shown by m_bar instead
    viewport()->setAutoFillBackground(false);               // the window's background shows

    m_content = new QWidget;
    m_content->setAutoFillBackground(false);
    auto *column = new QVBoxLayout(m_content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_barsSlot = new QWidget;
    auto *slotBox = new QVBoxLayout(m_barsSlot);
    slotBox->setContentsMargins(0, 0, 0, 0);
    m_barsSlot->hide();
    column->addWidget(m_barsSlot);
    auto *pageHolder = new QWidget;
    // Its margins place the page below the bars and beside the scroll bar (L9, SC2), so the
    // layout check doesn't read them as spacing.
    pageHolder->setProperty("wtPlacement", true);
    m_pageBox = new QVBoxLayout(pageHolder);
    m_pageBox->setContentsMargins(Design::kWindowMargin, 0, Design::kWindowMargin, 0);
    m_pageBox->setSpacing(0);
    column->addWidget(pageHolder, 1);
    setWidget(m_content);
    m_content->setAutoFillBackground(false);              // setWidget turns it on

    // SC2: the window's scroll bar mirrors the area's own (kept hidden).
    m_bar = new QScrollBar(Qt::Vertical);
    m_bar->setProperty("wtFloats", true);   // the layout check: it sits over the page's margin
    m_bar->hide();
    QScrollBar *own = verticalScrollBar();
    connect(own, &QScrollBar::rangeChanged, this, [this, own](int min, int max) {
        m_bar->setRange(min, max);
        m_bar->setPageStep(own->pageStep());
        m_bar->setSingleStep(own->singleStep());
        const bool show = max > min;
        if (show != m_bar->isVisibleTo(m_bar->parentWidget())) {
            m_bar->setVisible(show);
            emit barShownChanged(show);
        }
    });
    connect(own, &QScrollBar::valueChanged, m_bar, &QScrollBar::setValue);
    connect(m_bar, &QScrollBar::valueChanged, own, &QScrollBar::setValue);

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (now && m_content->isAncestorOf(now))
            keepFocusVisible(now);
    });
}

void PageArea::setPage(QWidget *page)
{
    m_pageBox->addWidget(page, 1);
}

void PageArea::setPageMargins(int top, int right, int bottom)
{
    m_pageBox->setContentsMargins(Design::kWindowMargin, top, right, bottom);
}

bool PageArea::focusNextPrevChild(bool next)
{
    // QScrollArea would scroll with its own 50 px margins; SC5's own rule runs instead.
    return QWidget::focusNextPrevChild(next);
}

void PageArea::keepFocusVisible(QWidget *w)
{
    QScrollBar *own = verticalScrollBar();
    if (own->maximum() <= 0)
        return;
    constexpr int spare = Design::kGapButtons;            // SC5: 8 px to spare
    const QRect r(w->mapTo(viewport(), QPoint(0, 0)), w->size());
    const int top = m_clearTop + spare;
    const int bottom = viewport()->height() - m_clearBottom - spare;
    int delta = 0;
    if (r.top() < top)
        delta = r.top() - top;                            // under the bars: bring it down
    else if (r.bottom() > bottom && r.height() <= bottom - top)
        delta = r.bottom() - bottom;                      // below: bring all of it up
    // (Taller than the page with its top showing: it stays where it is.)
    if (delta != 0)
        own->setValue(own->value() + delta);
}
