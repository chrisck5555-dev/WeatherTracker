#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include "StatsView.h"
#include "AppState.h"
#include "ClayPaint.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Stats.h"
#include "Units.h"

namespace {
constexpr int kRowHeight = 46;
constexpr int kMargin = 16;
constexpr int kLabelWidth = 340;
}

StatsView::StatsView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible()) reload();
    });
    connect(&m_state, &AppState::periodChanged, this, [this](Tab tab) {
        if (tab == Tab::Analytics && isVisible()) reload();
    });
}

void StatsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();
    QTimer::singleShot(0, this, [this] { updateHeight(); });   // measure again at the final width
}

void StatsView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateHeight();                 // wrapped text changes height with the width
}

void StatsView::reload()
{
    const Period p = m_state.period(Tab::Analytics);
    const QDate date = m_state.currentDate();
    const QDate from = PeriodMath::startOf(date, p);
    const QDate to = PeriodMath::endOf(date, p);

    const auto days = m_repo.weatherInRange(from, to);          // ONE query for the period
    const auto summary = Stats::summarize(days.values());
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));
    m_rows = StatsRows::build(summary, int(from.daysTo(to)) + 1, p == Period::Day, units);

    // Earlier-years panel (single day only): one more query for all earlier data.
    m_hasHistory = false;
    m_hasPeriodHistory = false;
    const auto first = m_repo.firstWeatherDate();
    if (first && first->year() < from.year()) {      // data from an earlier year exists
        if (p == Period::Day) {
            const auto earlier = m_repo.weatherInRange(*first, QDate(date.year() - 1, 12, 31));
            m_history = History::compareDay(date, m_repo.weatherFor(date), earlier, units);
            m_hasHistory = m_history.visible;
        } else {
            const auto earlier = m_repo.weatherInRange(*first, from.addDays(-1));
            m_periodHistory = History::comparePeriod(date, p, days, earlier, units);
            m_hasPeriodHistory = m_periodHistory.visible;
        }
    }
    
    updateHeight();
    update();
    QTimer::singleShot(0, this, [this] { updateHeight(); update(); });
}

void StatsView::updateHeight()
{
    setMinimumHeight(layoutContent(nullptr));      // the scroll area reads this
}

void StatsView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // No fill here: the window behind it paints the background.
    layoutContent(&painter);
}

// One function for measuring and drawing, so they can never disagree.
int StatsView::layoutContent(QPainter *p) const
{
    const QPalette pal = palette();
    QFont normal = font();
    normal.setPointSize(14);
    QFont bold = normal;
    bold.setBold(true);
    QFont big = bold;
    big.setPointSize(16);

    const int textW = qMax(200, width() - 2 * kMargin);
    double y = kMargin;

    // A word-wrapped paragraph. It advances y by its own measured height.
    auto paragraph = [&](const QString &text, const QFont &f, int indent) {
        const int w = textW - indent;
        const QFontMetrics fm(f, const_cast<StatsView *>(this));   // same device as the painter
        const int h = fm.boundingRect(0, 0, w, 100000, Qt::TextWordWrap, text).height() + 12;
        if (p) {
            p->setFont(f);
            p->setPen(pal.color(QPalette::WindowText));
            p->drawText(QRectF(kMargin + indent, y, w, h),
                        Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, text);
        }
        y += h;
    };

    if (m_rows.isEmpty())
        paragraph("Nothing recorded for this period yet.", normal, 0);

    for (const StatsRows::Row &row : m_rows) {
        const QRectF r(kMargin, y, textW, kRowHeight);
        if (p) {
            if (row.header) {
                p->setFont(bold);
                p->setPen(pal.color(QPalette::WindowText));
                p->drawText(r.adjusted(8, 8, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, row.label);
                p->setPen(pal.color(QPalette::Mid));
                p->drawLine(QPointF(r.left(), r.bottom() - 2), QPointF(r.right(), r.bottom() - 2));
            } else {
                ClayPaint::drawCard(*p, r.adjusted(0, 3, 0, -3), 14, pal.color(QPalette::Base));
                p->setPen(pal.color(QPalette::WindowText));
                p->setFont(normal);
                p->drawText(QRectF(r.left() + 16, r.top(), kLabelWidth, r.height()),
                            Qt::AlignVCenter | Qt::AlignLeft, row.label);
                p->setFont(bold);
                p->drawText(QRectF(r.left() + kLabelWidth + 24, r.top(),
                                   r.width() - kLabelWidth - 40, r.height()),
                            Qt::AlignVCenter | Qt::AlignLeft, row.value);
            }
        }
        y += kRowHeight;
    }

    auto comparison = [&](const QString &heading, const QStringList &highlights,
                          const QString &emptyMessage, const QList<History::YearBlock> &blocks) {
        y += 24;
        paragraph(heading, big, 0);
        for (const QString &h : highlights)
            paragraph("\u2022 " + h, normal, 8);
        if (blocks.isEmpty())
            paragraph(emptyMessage, normal, 0);
        for (const History::YearBlock &b : blocks) {
            paragraph(b.title, bold, 0);
            for (const QString &line : b.lines)
                paragraph(line, normal, 24);
        }
    };
    if (m_hasHistory)
        comparison("On this day in earlier years", m_history.highlights,
                   m_history.emptyMessage, m_history.blocks);
    if (m_hasPeriodHistory)
        comparison("Compared with earlier years", m_periodHistory.highlights,
                   m_periodHistory.emptyMessage, m_periodHistory.blocks);
    return int(y + 2 * kMargin);
}