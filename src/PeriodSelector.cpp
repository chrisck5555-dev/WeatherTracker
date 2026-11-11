#include "PeriodSelector.h"

PeriodSelector::PeriodSelector(const QString &label, const QList<Period> &options, QWidget *parent)
    : ChoiceButtons(label, parent)
{
    for (Period p : options)
        addChoice(static_cast<int>(p), periodName(p));     // id = the enum's number
    connect(this, &ChoiceButtons::choiceMade, this, [this](int id) {
        emit periodChosen(static_cast<Period>(id));
    });
}

void PeriodSelector::setPeriod(Period p)
{
    setChosen(static_cast<int>(p));
}
