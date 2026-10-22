#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

#include "ChartMath.h"
#include "Period.h"

// A line chart for one or more series (wind, pressure, humidity), drawn directly.
// It only draws what it is given.
class LineChart : public QWidget
{
    Q_OBJECT

public:
    explicit LineChart(QWidget *parent = nullptr);

    // colors[i] belongs to series.names[i]
    void setData(const QString &title, const ChartMath::LineSeries &series,
                 const QList<QColor> &colors, Period period);

protected:
    void paintEvent(QPaintEvent *) override;
    QSize minimumSizeHint() const override { return QSize(420, 280); }

private:
    QString m_title;
    ChartMath::LineSeries m_data;
    QList<QColor> m_colors;
    Period m_period = Period::Month;
};
