#pragma once

#include <QString>
#include <QWidget>

#include "ChartMath.h"

// A bar chart, drawn directly. It only draws what it is given.
class BarChart : public QWidget
{
    Q_OBJECT

public:
    explicit BarChart(QWidget *parent = nullptr);

    void setData(const QString &title, const ChartMath::BarSeries &series, const QColor &color);

protected:
    void paintEvent(QPaintEvent *) override;
    QSize minimumSizeHint() const override { return QSize(240, 140); }

private:
    QString m_title;
    ChartMath::BarSeries m_data;
    QColor m_color;
};