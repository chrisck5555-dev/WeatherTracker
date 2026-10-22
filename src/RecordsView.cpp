#include "RecordsView.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

#include "AppState.h"
#include "ClayPaint.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Units.h"

namespace {
constexpr int kRowHeight = 46;
constexpr int kMargin = 16;
constexpr int kLabelWidth = 330;
}

RecordsView::RecordsView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible()) reload();
    });
    connect(&m_state, &AppState::periodChanged, this, [this](Tab tab) {
        if (tab == Tab::Records && isVisible()) reload();
    });
}

void RecordsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();                                   // data may have changed while hidden
}

void RecordsView::reload()
{
    const Period p = m_state.period(Tab::Records);
    const QDate date = m_state.currentDate();
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));
    const auto days = m_repo.weatherInRange(PeriodMath::startOf(date, p),
                                            PeriodMath::endOf(date, p));   // ONE query
    m_rows = Records::build(days, units, m_state.today());
    setMinimumHeight(2 * kMargin + qMax(1, int(m_rows.size())) * kRowHeight);
    update();
}

int RecordsView::rowAt(const QPoint &pos) const
{
    const int row = (pos.y() - kMargin) / kRowHeight;
    if (pos.y() < kMargin || row < 0 || row >= m_rows.size())
        return -1;
    return row;
}

void RecordsView::mousePressEvent(QMouseEvent *event)
{
    m_pressedRow = rowAt(event->position().toPoint());
}

// Activates on release, over the same row that was pressed.
void RecordsView::mouseReleaseEvent(QMouseEvent *event)
{
    const int row = rowAt(event->position().toPoint());
    if (row >= 0 && row == m_pressedRow && m_rows[row].date.isValid())
        emit dayActivated(m_rows[row].date);
    m_pressedRow = -1;
}

void RecordsView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();     // no fill: the window behind paints the background

    QFont normal = font();
    normal.setPointSize(14);
    QFont bold = normal;
    bold.setBold(true);

    const int w = width() - 2 * kMargin;
    if (m_rows.isEmpty()) {
        p.setFont(normal);
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(kMargin, kMargin, w, kRowHeight), Qt::AlignVCenter | Qt::AlignLeft,
                   "Nothing recorded for this period yet.");
        return;
    }

    for (int i = 0; i < m_rows.size(); ++i) {
        const Records::Item &row = m_rows[i];
        const QRectF r(kMargin, kMargin + i * kRowHeight, w, kRowHeight);
        p.setPen(pal.color(QPalette::WindowText));
        if (row.header) {
            p.setFont(bold);
            p.drawText(r.adjusted(8, 8, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, row.label);
            p.setPen(pal.color(QPalette::Mid));
            p.drawLine(QPointF(r.left(), r.bottom() - 2), QPointF(r.right(), r.bottom() - 2));
            continue;
        }
        ClayPaint::drawCard(p, r.adjusted(0, 3, 0, -3), 14, pal.color(QPalette::Base));
        p.setFont(normal);
        p.drawText(QRectF(r.left() + 16, r.top(), kLabelWidth, r.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, row.label);

        const bool link = row.date.isValid();
        const double valueLeft = r.left() + kLabelWidth + 24;
        const double valueWidth = r.right() - valueLeft - (link ? 36 : 16);
        p.setFont(bold);
        const QString text = QFontMetrics(bold, this).elidedText(row.value, Qt::ElideRight,
                                                                  int(valueWidth));
        p.drawText(QRectF(valueLeft, r.top(), valueWidth, r.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, text);
        if (link) {                                     // a day is behind this row
            p.setPen(pal.color(QPalette::Mid));
            p.setFont(bold);
            p.drawText(QRectF(r.right() - 32, r.top(), 24, r.height()),
                       Qt::AlignCenter, QStringLiteral("›"));
        }
    }
}
