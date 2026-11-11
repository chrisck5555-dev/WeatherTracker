#pragma once

#include <QList>

#include "ChoiceButtons.h"
#include "Period.h"

// A tab's period choice, at the right end of the tab row (L10): "Show:", "Numbers for:",
// "Chart for:" or "Records for:" with its periods.
class PeriodSelector : public ChoiceButtons
{
    Q_OBJECT

public:
    PeriodSelector(const QString &label, const QList<Period> &options, QWidget *parent = nullptr);

    void setPeriod(Period p);      // highlight the matching button

signals:
    void periodChosen(Period p);   // the user clicked a button, or moved to one with the keyboard
};
