#include "PeriodSelector.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QPushButton>

#include "KeyboardNav.h"

PeriodSelector::PeriodSelector(const QString &label, const QList<Period> &options,
                               QWidget *parent)
    : QWidget(parent)
    , m_group(new QButtonGroup(this))
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel(label));

    for (Period p : options) {
        auto *button = new QPushButton(periodName(p));
        button->setCheckable(true);              // stays "pressed" when chosen
        button->setMinimumHeight(44);
        m_group->addButton(button, static_cast<int>(p));   // id = the enum's number
        row->addWidget(button);
        button->installEventFilter(this);        // arrow keys move between the choices
    }
    row->addStretch(1);                          // push buttons to the left

    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        emit periodChosen(static_cast<Period>(id));
    });
}

// Like the tab row: while one of these buttons has the focus, Left/Right choose the
// neighbouring period and Home/End the first/last. The window would otherwise turn
// Left/Right into Previous/Next, so the buttons claim those keys first.
bool PeriodSelector::eventFilter(QObject *watched, QEvent *event)
{
    const auto keys = {int(Qt::Key_Left), int(Qt::Key_Right), int(Qt::Key_Home), int(Qt::Key_End)};
    if (KeyboardNav::claim(event, keys))
        return true;
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto *key = static_cast<QKeyEvent *>(event);
    const int k = key->key();
    if ((key->modifiers() & ~Qt::KeypadModifier)
        || (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Home && k != Qt::Key_End))
        return QWidget::eventFilter(watched, event);

    const QList<QAbstractButton *> buttons = m_group->buttons();   // in the order shown
    const int at = buttons.indexOf(qobject_cast<QAbstractButton *>(watched));
    if (at < 0)
        return false;
    const int to = k == Qt::Key_Home ? 0
                 : k == Qt::Key_End  ? int(buttons.size()) - 1
                 : k == Qt::Key_Left ? qMax(0, at - 1)
                                     : qMin(int(buttons.size()) - 1, at + 1);
    QAbstractButton *target = buttons[to];
    target->setFocus(Qt::TabFocusReason);        // keeps the keyboard outline showing
    if (!target->isChecked())
        target->click();                         // chooses it, as a mouse click would
    return true;
}

void PeriodSelector::setPeriod(Period p)
{
    if (auto *button = m_group->button(static_cast<int>(p)))
        button->setChecked(true);
}