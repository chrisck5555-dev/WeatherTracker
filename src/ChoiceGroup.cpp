#include "ChoiceGroup.h"

#include <QApplication>

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QVBoxLayout>
#include <algorithm>

#include "Design.h"
#include "KeyboardNav.h"

namespace {

std::function<QPixmap(const QString &, int, qreal)> &pictureSource()
{
    static std::function<QPixmap(const QString &, int, qreal)> source;
    return source;
}

constexpr int kPicture = 24;                    // Z3: standing alone (D65)
constexpr int kPad = 4;                         // S1: the cell's edge to its picture and words
constexpr int kPictureToWords = 4;              // 4 + 24 + 4 + 20 (Caption) + 4 = 56

// The compass (K10): NW N NE / W . E / SW S SE.
QPoint compassPlace(const QString &side)
{
    static const QStringList order{"NW", "N", "NE", "W", "", "E", "SW", "S", "SE"};
    const int i = int(order.indexOf(side));
    return i < 0 ? QPoint(0, 0) : QPoint(i % 3, i / 3);
}

} // namespace

// --- ChoiceCell ----------------------------------------------------------------------------

ChoiceCell::ChoiceCell(const Choice &choice, QWidget *parent)
    : QPushButton(parent), m_choice(choice)
{
    setCheckable(true);
    setFixedSize(ChoiceGroup::kCellWidth, ChoiceGroup::kCellHeight);
    setAccessibleName(choice.spoken.isEmpty() ? choice.words : choice.spoken);
}

QSize ChoiceCell::sizeHint() const { return QSize(ChoiceGroup::kCellWidth, ChoiceGroup::kCellHeight); }

void ChoiceCell::paintEvent(QPaintEvent *)
{
    QStylePainter p(this);
    QStyleOptionButton o;
    initStyleOption(&o);
    o.text.clear();
    o.icon = QIcon();
    p.drawControl(QStyle::CE_PushButtonBevel, o);      // fill, tints and outline (ClayStyle)

    const bool on = isChecked();
    const QPixmap pm = ChoiceGroup::picture(m_choice.picture, kPicture, devicePixelRatioF());
    if (!pm.isNull())
        p.drawPixmap(QPoint((width() - kPicture) / 2, kPad), pm);

    // The words in Caption (T6), Semibold when chosen like every chosen button's label.
    QFont f = font();
    const Design::TypeSpec t = Design::type(Design::TextStyle::Caption);
    f.setPixelSize(t.px);
    f.setWeight(on ? QFont::DemiBold : QFont::Normal);
    p.setFont(f);
    const QPalette::ColorGroup g = isEnabled() ? QPalette::Active : QPalette::Disabled;
    p.setPen(palette().color(g, on && isEnabled() ? QPalette::HighlightedText : QPalette::ButtonText));
    const QRect words(4, kPad + kPicture + kPictureToWords, width() - 8, t.lineHeight);
    p.drawText(words, Qt::AlignHCenter | Qt::AlignVCenter, m_choice.words);
}

// --- ChoiceGroup ---------------------------------------------------------------------------

ChoiceGroup::ChoiceGroup(const QString &label, const QString &spokenName,
                         const QList<Choice> &choices, Shape shape, int fullColumns, QWidget *parent)
    : QWidget(parent), m_shape(shape), m_fullColumns(shape == Shape::Compass ? 3 : fullColumns)
{
    setAccessibleName(spokenName);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(Design::kGapHeader);                 // S2: a control and its header

    m_header = new QHBoxLayout;
    m_header->setContentsMargins(0, 0, 0, 0);
    m_header->setSpacing(Design::kGapLabel);
    m_label = new QLabel(label);
    m_label->setMinimumHeight(Design::kTarget);
    m_header->addWidget(m_label);
    m_clear = new QPushButton("Clear");
    m_clear->setProperty("wtKind", "quiet");                // D42: quiet, beside the label
    m_clear->setAccessibleName("Clear: " + spokenName);
    m_clear->setToolTip("Remove the choice, so nothing is recorded for it.");
    m_header->addWidget(m_clear);
    m_header->addStretch(1);
    column->addLayout(m_header);

    m_grid = new QWidget;
    m_gridLayout = new QGridLayout(m_grid);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(kCellGap);
    column->addWidget(m_grid, 0, Qt::AlignLeft);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (const Choice &c : choices) {
        auto *cell = new ChoiceCell(c, m_grid);
        group->addButton(cell);
        cell->installEventFilter(this);
        cell->setProperty("wtOwnArrows", true);             // the arrows move inside (KB2)
        m_cells << cell;
        connect(cell, &QPushButton::clicked, this, [this] {
            updateState();
            emit changed();
        });
    }
    connect(m_clear, &QPushButton::clicked, this, [this] {
        const bool keyboard = m_clear->hasFocus();
        clear();
        if (!m_cells.isEmpty())                             // Clear is now disabled: stay here
            m_cells.first()->setFocus(keyboard ? Qt::TabFocusReason : Qt::OtherFocusReason);
    });
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    place();
    updateState();
}

QString ChoiceGroup::value() const
{
    for (ChoiceCell *c : m_cells)
        if (c->isChecked())
            return c->choice().value;
    return QString();
}

ChoiceCell *ChoiceGroup::cell(const QString &value) const
{
    for (ChoiceCell *c : m_cells)
        if (c->choice().value == value)
            return c;
    return nullptr;
}

void ChoiceGroup::setValue(const QString &value)
{
    ChoiceCell *target = value.isEmpty() ? nullptr : cell(value);
    if (target) {
        target->setChecked(true);
    } else if (auto *g = m_cells.isEmpty() ? nullptr : m_cells.first()->group()) {
        g->setExclusive(false);                             // an exclusive group can't empty itself
        for (ChoiceCell *c : m_cells)
            c->setChecked(false);
        g->setExclusive(true);
    }
    updateState();
}

void ChoiceGroup::clear()
{
    if (value().isEmpty())
        return;
    setValue(QString());
    emit changed();
}

void ChoiceGroup::addToHeader(QWidget *w)
{
    m_header->insertWidget(m_header->count() - 1, w, 1);    // before the stretch
}

int ChoiceGroup::columns() const { return m_narrow ? kNarrowColumns : m_fullColumns; }

int ChoiceGroup::rows() const
{
    if (m_shape == Shape::Compass)
        return 3;
    const int n = int(m_cells.size()), c = columns();
    return (n + c - 1) / c;
}

void ChoiceGroup::setNarrow(bool narrow)
{
    if (narrow == m_narrow)
        return;
    m_narrow = narrow;
    place();
}

void ChoiceGroup::place()
{
    for (ChoiceCell *c : m_cells)
        m_gridLayout->removeWidget(c);
    const int cols = columns();
    for (int i = 0; i < m_cells.size(); ++i) {
        ChoiceCell *c = m_cells[i];
        const QPoint at = m_shape == Shape::Compass ? compassPlace(c->choice().value)
                                                    : QPoint(i % cols, i / cols);
        c->column = at.x();
        c->row = at.y();
        m_gridLayout->addWidget(c, c->row, c->column);
    }
    // QGridLayout gives no room to an empty row or column, so the height is set outright.
    // The width it may shrink to is always 3 columns' worth, whatever the layout now, so a
    // page that holds full grids can still narrow to the point where they switch (D43);
    // fitAll() switches them in the same resize.
    for (int x = 0; x < 5; ++x)
        m_gridLayout->setColumnMinimumWidth(x, x < cols ? kCellWidth : 0);
    const int r = rows();
    const int gridHeight = r * kCellHeight + (r - 1) * kCellGap;
    m_grid->setFixedHeight(gridHeight);
    m_grid->setMinimumWidth(widthFor(kNarrowColumns));
    m_grid->setMaximumWidth(widthFor(cols));
    // At least the label row and the grid, outright, so a page that has just switched layouts
    // never squeezes the group for an event.
    setMinimumHeight(Design::kTarget + Design::kGapHeader + gridHeight);
    updateGeometry();
}

void ChoiceGroup::updateState()
{
    ChoiceCell *stop = m_roving;                            // while the arrows are moving inside
    for (ChoiceCell *c : m_cells)
        if (!stop && c->isChecked())
            stop = c;
    if (!stop && !m_cells.isEmpty())
        stop = m_cells.first();                             // KB2: nothing chosen yet
    for (ChoiceCell *c : m_cells)
        c->setFocusPolicy(c == stop ? Qt::StrongFocus : Qt::ClickFocus);
    const bool any = !value().isEmpty();
    if (!any && m_clear->hasFocus() && stop)
        stop->setFocus(Qt::OtherFocusReason);
    m_clear->setEnabled(any);                               // D42: only when there is a choice
}

ChoiceCell *ChoiceGroup::neighbour(ChoiceCell *from, int key) const
{
    QList<ChoiceCell *> order;                              // reading order, enabled cells only
    for (ChoiceCell *c : m_cells)
        if (c->isEnabled())
            order << c;
    std::sort(order.begin(), order.end(), [](ChoiceCell *a, ChoiceCell *b) {
        return a->row != b->row ? a->row < b->row : a->column < b->column;
    });
    const int at = int(order.indexOf(from));
    if (at < 0 || order.isEmpty())
        return nullptr;
    switch (key) {
    case Qt::Key_Home:  return order.first();
    case Qt::Key_End:   return order.last();
    case Qt::Key_Left:  return order[std::max(0, at - 1)];
    case Qt::Key_Right: return order[std::min(int(order.size()) - 1, at + 1)];
    case Qt::Key_Up:
    case Qt::Key_Down: {
        // The nearest cell in the same column, skipping the compass's empty centre.
        const int step = key == Qt::Key_Up ? -1 : 1;
        for (int r = from->row + step; r >= 0 && r < 8; r += step)
            for (ChoiceCell *c : order)
                if (c->row == r && c->column == from->column)
                    return c;
        return from;
    }
    default:
        return nullptr;
    }
}

bool ChoiceGroup::eventFilter(QObject *watched, QEvent *event)
{
    auto *cell = qobject_cast<ChoiceCell *>(watched);
    // Focus leaving the group: Tab comes back in at the chosen cell next time (KB2).
    if (event->type() == QEvent::FocusOut && m_roving
        && !m_cells.contains(qobject_cast<ChoiceCell *>(QApplication::focusWidget()))) {
        m_roving = nullptr;
        updateState();
    }
    const auto keys = {int(Qt::Key_Left), int(Qt::Key_Right), int(Qt::Key_Up), int(Qt::Key_Down),
                       int(Qt::Key_Home), int(Qt::Key_End)};
    if (KeyboardNav::claim(event, keys))
        return true;
    if (event->type() != QEvent::KeyPress || !cell)
        return QWidget::eventFilter(watched, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() & ~Qt::KeypadModifier)
        return QWidget::eventFilter(watched, event);
    // Enter chooses, as Space does (KB3).
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (!cell->isChecked())
            cell->click();
        return true;
    }
    ChoiceCell *target = neighbour(cell, key->key());
    if (!target)
        return QWidget::eventFilter(watched, event);
    // The arrows only move the focus tint; Space or Enter chooses (KB2, KB3; Chris,
    // 2026-11-13: arrows had been choosing as they moved).
    // The cell they reach is the group's Tab stop until focus leaves, so Tab goes on.
    m_roving = target;
    updateState();
    target->setFocus(Qt::TabFocusReason);                   // keeps the focus tint showing (F1)
    return true;
}

void ChoiceGroup::fitAll(const QList<ChoiceGroup *> &groups, int available)
{
    int widest = 0;
    for (const ChoiceGroup *g : groups)
        widest = std::max(widest, g->fullWidth());
    const bool narrow = available < widest;
    for (ChoiceGroup *g : groups)
        g->setNarrow(narrow);
}

void ChoiceGroup::setPictureSource(std::function<QPixmap(const QString &, int, qreal)> source)
{
    pictureSource() = std::move(source);
}

QPixmap ChoiceGroup::picture(const QString &name, int size, qreal dpr)
{
    if (name.isEmpty() || !pictureSource())
        return QPixmap();
    return pictureSource()(name, size, dpr);
}
