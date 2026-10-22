#pragma once

#include <QWidget>

#include "ChartMath.h"
#include "Period.h"
#include "Units.h"

// The temperature line chart, drawn directly. It only draws what it is given.
class TempChart : public QWidget
{
    Q_OBJECT

public:
    explicit TempChart(QWidget *parent = nullptr);

    void setData(const ChartMath::TempSeries &series, Period period, Units::System units);

protected:
    void paintEvent(QPaintEvent *) override;
    QSize minimumSizeHint() const override { return QSize(420, 280); }

private:
    ChartMath::TempSeries m_data;
    Period m_period = Period::Month;
    Units::System m_units = Units::System::Metric;
};