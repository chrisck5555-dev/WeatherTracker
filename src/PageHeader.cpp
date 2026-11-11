#include "PageHeader.h"

#include <QDate>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QPushButton>
#include <QStringList>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <algorithm>

#include "Design.h"
#include "FlowLayout.h"
#include "Icons.h"
#include "PeriodMath.h"
#include "ThemeManager.h"
#include "Typography.h"

// A title in cells (T5). Each cell is drawn on its own: a cell with a width is that wide
// (its text right- or left-aligned in it), so whatever follows it starts at the same pixel
// whatever the text inside. A cell with width < 0 is as wide as its text.
struct ColumnText {
    struct Cell {
        QString text;
        int width = -1;
        Qt::Alignment align = Qt::AlignLeft;
    };
    QList<Cell> cells;

    QString plain() const
    {
        QString t;
        for (const Cell &c : cells)
            t += c.text;
        return t;
    }

    int width(const QFontMetrics &fm) const
    {
        int w = 0;
        for (const Cell &c : cells)
            w += c.width >= 0 ? c.width : fm.horizontalAdvance(c.text);
        return w;
    }

    // Draws the cells from `left`, centred in `top`..`top + height`; returns where they end.
    int paint(QPainter &p, int left, int top, int height) const
    {
        const QFontMetrics fm(p.font());
        int x = left;
        for (const Cell &c : cells) {
            const int w = c.width >= 0 ? c.width : fm.horizontalAdvance(c.text);
            // One more px than the text needs, so the last letter is never clipped.
            const QRect r = c.align & Qt::AlignRight ? QRect(x - 1, top, w + 1, height)
                                                     : QRect(x, top, w + 1, height);
            p.drawText(r, c.align | Qt::AlignVCenter, c.text);
            x += w;
        }
        return x;
    }
};

// The title when it zooms out: a quiet button (B1) with the up arrow after the text. ClayStyle
// draws its shape, hover and focus; the text is drawn here, in columns.
class TitleButton : public QPushButton
{
public:
    TitleButton()
    {
        setProperty("wtKind", "quiet");
        setIcon(Icons::icon("arrow_up"));          // for the layout check and Narrator
    }

    void setColumns(const ColumnText &text)
    {
        m_text = text;
        setText(text.plain());
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        // As ClayStyle sizes a button: icon, 8, text, and 16 each side; at least 44 (Z1, S1).
        const QFontMetrics fm(font());
        const int content = Design::kIconInText + Design::kGapButtons + m_text.width(fm);
        return QSize(std::max(Design::kTarget, content + 2 * Design::kPadCard),
                     std::max(Design::kTarget, fm.height() + 2 * Design::kGapButtons));
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter p(this);
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = QIcon();
        p.drawControl(QStyle::CE_PushButtonBevel, option);   // shape, hover, focus tint

        const auto c = ThemeManager::instance()->colors();
        const QColor color = isEnabled() ? c.text : c.disabledText;
        p.setFont(font());
        p.setPen(color);
        const int end = m_text.paint(p, Design::kPadCard, 0, height());
        const int s = Design::kIconInText;
        p.drawPixmap(QPoint(end + Design::kGapButtons, (height() - s) / 2),
                     Icons::pixmap("arrow_up", s, color, false, devicePixelRatioF()));
    }

private:
    ColumnText m_text;
};

// The title as plain text, indented as far as the button's text so the two line up.
class TitleLabel : public QLabel
{
public:
    void setColumns(const ColumnText &text)
    {
        m_text = text;
        setText(text.plain());
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        const QMargins m = contentsMargins();
        const QFontMetrics fm(font());
        return QSize(m.left() + m_text.width(fm) + m.right(),
                     std::max(Design::kTarget, fm.height() + m.top() + m.bottom()));
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setFont(font());
        p.setPen(ThemeManager::instance()->colors().text);
        const QRect r = contentsRect();
        m_text.paint(p, r.left(), r.top(), r.height());
    }

private:
    ColumnText m_text;
};

PageHeader::PageHeader(QWidget *parent)
    : QWidget(parent)
{
    m_flow = new FlowLayout(this, Design::kGapButtons, Design::kGapButtons);

    // Previous, Next and Today stay together (L5).
    m_nav = new QWidget;
    auto *nav = new QHBoxLayout(m_nav);
    nav->setContentsMargins(0, 0, 0, 0);
    nav->setSpacing(Design::kGapButtons);
    m_prev = new QPushButton("Previous");
    m_prev->setIcon(Icons::icon("chevron_left"));
    m_next = new QPushButton("Next");
    m_next->setIcon(Icons::icon("chevron_right"));
    m_next->setProperty("wtIconAfter", true);
    m_today = new QPushButton("Today");
    m_today->setIcon(Icons::icon("calendar_today"));
    for (QPushButton *b : {m_prev, m_next, m_today})
        nav->addWidget(b);
    QSizePolicy keep = m_nav->sizePolicy();
    keep.setRetainSizeWhenHidden(true);            // "All time": the title doesn't jump left
    m_nav->setSizePolicy(keep);

    // The title: a quiet button (zooms out) or plain text, both in the Title style (T6), with
    // same-width digits (T5).
    m_titleButton = new TitleButton;
    m_titleButton->setFont(tabularDigits(textFont(Design::TextStyle::Title, m_titleButton->font())));
    m_titleLabel = new TitleLabel;
    m_titleLabel->setFont(tabularDigits(textFont(Design::TextStyle::Title, m_titleLabel->font())));
    m_titleLabel->setContentsMargins(Design::kGapCards, 0, Design::kGapCards, 0);
    m_titleLabel->setMinimumHeight(Design::kTarget);
    m_titleButton->hide();

    m_back = new QPushButton;
    m_back->setIcon(Icons::icon("arrow_left"));
    m_back->hide();

    m_flow->addWidget(m_nav);
    m_flow->addWidget(m_titleButton);
    m_flow->addWidget(m_titleLabel);
    m_flow->addWidget(m_back, FlowLayout::PushRight);
    // Its height comes from heightForWidth: a Fixed policy would cap it at one line.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(m_titleButton, &QPushButton::clicked, this, &PageHeader::zoomOutRequested);
}

void PageHeader::setPeriodTitle(const QDate &date, Period period, const QString &zoomTip)
{
    // The words are English, as PeriodMath::title writes them (QDate::toString).
    QStringList months, weekdays;
    for (int m = 1; m <= 12; ++m)
        months << QDate(2026, m, 1).toString("MMMM");
    for (int i = 0; i < 7; ++i)                     // Nov 1, 2026 is a Sunday; any week will do
        weekdays << QDate(2026, 11, 1 + i).toString("dddd");

    // The cells: the words that change as she steps, right-aligned in a slot as wide as their
    // widest form; the day, right-aligned in a slot as wide as the widest day number; the
    // year, in a slot as wide as four of the widest digit. So every number, and the up arrow
    // after the title, keeps its place whatever the words or digits.
    const QFontMetrics fm(m_titleButton->font());
    auto widest = [&fm](const QStringList &forms) {
        int w = 0;
        for (const QString &f : forms)
            w = std::max(w, fm.horizontalAdvance(f));
        return w;
    };
    QStringList days, digits;
    for (int d = 1; d <= 31; ++d)
        days << QString::number(d);
    for (char c = '0'; c <= '9'; ++c)
        digits << QString(4, QChar(c));
    const int dayW = widest(days), yearW = widest(digits);
    using Cell = ColumnText::Cell;
    const Cell gap{" "};
    ColumnText text;
    const QDate start = PeriodMath::startOf(date, period);
    switch (period) {
    case Period::Day: {                                  // "Tuesday, November" 10, 2026
        QStringList forms;
        for (const QString &w : weekdays)
            for (const QString &m : months)
                forms << w + ", " + m;
        text.cells = {{date.toString("dddd, MMMM"), widest(forms), Qt::AlignRight}, gap,
                      {QString::number(date.day()), dayW, Qt::AlignRight}, {","}, gap,
                      {QString::number(date.year()), yearW, Qt::AlignLeft}};
        break;
    }
    case Period::Week: {                                 // "Week of November" 8, 2026
        QStringList forms;
        for (const QString &m : months)
            forms << "Week of " + m;
        text.cells = {{"Week of " + start.toString("MMMM"), widest(forms), Qt::AlignRight}, gap,
                      {QString::number(start.day()), dayW, Qt::AlignRight}, {","}, gap,
                      {QString::number(start.year()), yearW, Qt::AlignLeft}};
        break;
    }
    case Period::Month:                                  // "November" 2026
        text.cells = {{date.toString("MMMM"), widest(months), Qt::AlignRight}, gap,
                      {QString::number(date.year()), yearW, Qt::AlignLeft}};
        break;
    case Period::Season: {                               // "Winter" 2026–27
        const QString title = PeriodMath::title(date, period);
        text.cells = {{title.section(' ', 0, 0), widest({"Winter", "Spring", "Summer", "Fall"}),
                       Qt::AlignRight},
                      gap, {title.section(' ', 1)}};
        break;
    }
    case Period::Year:                                   // 2026
        text.cells = {{QString::number(date.year()), yearW, Qt::AlignLeft}};
        break;
    case Period::All:
        setTitle(PeriodMath::title(date, period), zoomTip);
        return;
    }
    showTitle(text, PeriodMath::title(date, period), zoomTip);
}

void PageHeader::showTitle(const ColumnText &text, const QString &plain, const QString &zoomTip)
{
    const bool zoom = !zoomTip.isEmpty();
    m_titleButton->setColumns(text);
    m_titleButton->setToolTip(zoomTip);
    // Narrator and the tooltip read the plain title, without the padding.
    m_titleButton->setAccessibleName(zoom ? plain + ". " + zoomTip : plain);
    m_titleLabel->setColumns(text);
    m_titleLabel->setAccessibleName(plain);
    m_titleButton->setVisible(zoom);
    m_titleLabel->setVisible(!zoom);
}

void PageHeader::setTitle(const QString &text, const QString &zoomTip)
{
    ColumnText plainText;
    plainText.cells = {{text}};
    showTitle(plainText, text, zoomTip);
}

QPushButton *PageHeader::titleButton() const { return m_titleButton; }

void PageHeader::setNavigationShown(bool shown)
{
    m_nav->setVisible(shown);
}

void PageHeader::setBack(const QString &text)
{
    m_back->setText(text);
    m_back->setVisible(!text.isEmpty());
}

QSize PageHeader::sizeHint() const { return m_flow->sizeHint(); }
QSize PageHeader::minimumSizeHint() const { return m_flow->minimumSize(); }
int PageHeader::heightForWidth(int width) const { return m_flow->heightForWidth(width); }
