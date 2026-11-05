#pragma once

#include <QList>
#include <QWidget>

#include "Period.h"

class QButtonGroup;

class PeriodSelector : public QWidget
{
    Q_OBJECT

public:
    PeriodSelector(const QString &label, const QList<Period> &options,
                   QWidget *parent = nullptr);

    void setPeriod(Period p);      // highlight the matching button

signals:
    void periodChosen(Period p);   // the user clicked a button, or moved to one with the keyboard

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QButtonGroup *m_group;
};