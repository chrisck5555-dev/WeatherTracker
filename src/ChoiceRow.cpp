#include "ChoiceRow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QPushButton>

#include "Design.h"
#include "FlowLayout.h"
#include "KeyboardNav.h"

ChoiceRow::ChoiceRow(const QString &spokenName, QWidget *parent)
    : QWidget(parent)
    , m_flow(new FlowLayout(this, Design::kGapButtons, Design::kGapButtons))   // S2: 8
    , m_group(new QButtonGroup(this))
{
    m_flow->setContentsMargins(0, 0, 0, 0);
    setAccessibleName(spokenName);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        m_roving = nullptr;
        updateStops();
        emit chosenChanged(id);
    });
}

QPushButton *ChoiceRow::addChoice(int id, const QString &words, const QString &tooltip)
{
    auto *b = new QPushButton(words, this);
    b->setCheckable(true);
    b->setToolTip(tooltip);
    b->setAccessibleName(QString(words).replace('\n', ", "));
    b->installEventFilter(this);
    b->setProperty("wtOwnArrows", true);                  // the arrows move inside (KB2)
    m_group->addButton(b, id);
    m_buttons.append(b);
    m_flow->addWidget(b);
    // As wide as its words, but never under 88, so short words still make a broad target;
    // all as tall as the tallest, at least 44 (Z1). Natural widths keep a row short enough to
    // sit beside another (Chris, 2026-11-13: Units beside Text size, Style beside Brightness).
    int h = Design::kTarget;
    for (QPushButton *each : m_buttons)
        h = std::max(h, each->sizeHint().height());
    for (QPushButton *each : m_buttons)
        each->setFixedSize(std::max(2 * Design::kTarget, each->sizeHint().width()), h);
    updateStops();
    return b;
}

void ChoiceRow::setChosen(int id)
{
    if (QAbstractButton *b = m_group->button(id))
        b->setChecked(true);
    m_roving = nullptr;
    updateStops();
}

int ChoiceRow::chosen() const
{
    return m_group->checkedId();
}

void ChoiceRow::updateStops()
{
    QPushButton *stop = m_roving;
    if (!stop)
        stop = qobject_cast<QPushButton *>(m_group->checkedButton());
    if (!stop && !m_buttons.isEmpty())
        stop = m_buttons.first();
    for (QPushButton *b : m_buttons)
        b->setFocusPolicy(b == stop ? Qt::StrongFocus : Qt::ClickFocus);
}

bool ChoiceRow::eventFilter(QObject *watched, QEvent *event)
{
    auto *b = qobject_cast<QPushButton *>(watched);
    const int at = b ? int(m_buttons.indexOf(b)) : -1;
    if (at < 0)
        return QWidget::eventFilter(watched, event);
    // Focus leaving the row: Tab comes back in at the chosen button next time (KB2).
    if (event->type() == QEvent::FocusOut && m_roving
        && !m_buttons.contains(qobject_cast<QPushButton *>(QApplication::focusWidget()))) {
        m_roving = nullptr;
        updateStops();
    }
    const auto keys = {int(Qt::Key_Left), int(Qt::Key_Right), int(Qt::Key_Up), int(Qt::Key_Down),
                       int(Qt::Key_Home), int(Qt::Key_End)};
    if (KeyboardNav::claim(event, keys))
        return true;
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() & ~Qt::KeypadModifier)
        return QWidget::eventFilter(watched, event);
    const int last = int(m_buttons.size()) - 1;
    int to = at;
    switch (key->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:    to = std::max(0, at - 1); break;
    case Qt::Key_Right:
    case Qt::Key_Down:  to = std::min(last, at + 1); break;
    case Qt::Key_Home:  to = 0; break;
    case Qt::Key_End:   to = last; break;
    default:            return QWidget::eventFilter(watched, event);
    }
    // D66: the arrows only move the focus; Space or Enter chooses.
    m_roving = m_buttons[to];
    updateStops();
    m_roving->setFocus(Qt::TabFocusReason);              // keeps the focus tint showing (F1)
    return true;
}
