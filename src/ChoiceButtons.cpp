#include "ChoiceButtons.h"

#include <QButtonGroup>
#include <algorithm>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>

#include "Design.h"
#include "FlowLayout.h"
#include "Icons.h"
#include "KeyboardNav.h"

ChoiceButtons::ChoiceButtons(const QString &label, QWidget *parent)
    : QWidget(parent), m_group(new QButtonGroup(this))
{
    m_group->setExclusive(true);
    m_flow = new FlowLayout(this, Design::kGapButtons, Design::kGapButtons);
    m_flow->setGridWhenWrapped(true);
    if (!label.isEmpty()) {
        m_label = new QLabel(label);
        m_label->setMinimumHeight(Design::kTarget);
        m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_flow->addWidget(m_label, FlowLayout::Header);
    }
    // Its height comes from heightForWidth: a Fixed policy would cap it at one line.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        updateTabStop();
        emit choiceMade(id);
    });
}

QPushButton *ChoiceButtons::addChoice(int id, const QString &text, const QString &icon)
{
    auto *b = new QPushButton(text);
    b->setCheckable(true);
    b->setMinimumHeight(Design::kTarget);
    if (!icon.isEmpty())
        b->setIcon(Icons::icon(icon));
    m_group->addButton(b, id);
    m_flow->addWidget(b);
    b->installEventFilter(this);
    if (m_group->buttons().size() == 1)
        b->setChecked(true);
    updateTabStop();
    return b;
}

void ChoiceButtons::setChosen(int id)
{
    if (QAbstractButton *b = m_group->button(id))
        b->setChecked(true);
    updateTabStop();
}

int ChoiceButtons::chosen() const { return m_group->checkedId(); }

QPushButton *ChoiceButtons::button(int id) const
{
    return qobject_cast<QPushButton *>(m_group->button(id));
}

QList<QAbstractButton *> ChoiceButtons::buttons() const { return m_group->buttons(); }

// KB2: only the chosen button is in the Tab order; the others still take a click.
void ChoiceButtons::updateTabStop()
{
    QAbstractButton *checked = m_group->checkedButton();
    for (QAbstractButton *b : m_group->buttons())
        b->setFocusPolicy(b == checked ? Qt::StrongFocus : Qt::ClickFocus);
}

QSize ChoiceButtons::sizeHint() const { return m_flow->sizeHint(); }
QSize ChoiceButtons::minimumSizeHint() const { return m_flow->minimumSize(); }
int ChoiceButtons::heightForWidth(int width) const { return m_flow->heightForWidth(width); }

bool ChoiceButtons::eventFilter(QObject *watched, QEvent *event)
{
    const auto keys = {int(Qt::Key_Left), int(Qt::Key_Right), int(Qt::Key_Up), int(Qt::Key_Down),
                       int(Qt::Key_Home), int(Qt::Key_End)};
    if (KeyboardNav::claim(event, keys))
        return true;
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto *key = static_cast<QKeyEvent *>(event);
    const int k = key->key();
    if (key->modifiers() & ~Qt::KeypadModifier)
        return QWidget::eventFilter(watched, event);
    const bool back = k == Qt::Key_Left || k == Qt::Key_Up;
    const bool forward = k == Qt::Key_Right || k == Qt::Key_Down;
    if (!back && !forward && k != Qt::Key_Home && k != Qt::Key_End)
        return QWidget::eventFilter(watched, event);

    QList<QAbstractButton *> list;                       // the enabled ones, in the order shown
    for (QAbstractButton *b : m_group->buttons())
        if (b->isEnabled() && b->isVisible())
            list << b;
    const int at = int(list.indexOf(qobject_cast<QAbstractButton *>(watched)));
    if (at < 0 || list.isEmpty())
        return false;
    const int to = k == Qt::Key_Home ? 0
                 : k == Qt::Key_End  ? int(list.size()) - 1
                 : back              ? std::max(0, at - 1)
                                     : std::min(int(list.size()) - 1, at + 1);
    QAbstractButton *target = list[to];
    target->setFocus(Qt::TabFocusReason);               // keeps the focus tint showing (F1)
    if (!target->isChecked())
        target->click();                                // chooses it, as a click would
    return true;
}
