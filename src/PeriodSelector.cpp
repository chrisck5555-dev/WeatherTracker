#include "PeriodSelector.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

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
    }
    row->addStretch(1);                          // push buttons to the left

    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        emit periodChosen(static_cast<Period>(id));
    });
}

void PeriodSelector::setPeriod(Period p)
{
    if (auto *button = m_group->button(static_cast<int>(p)))
        button->setChecked(true);
}