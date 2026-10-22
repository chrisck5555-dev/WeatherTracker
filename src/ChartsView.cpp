#include "ChartsView.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "AppState.h"
#include "BarChart.h"
#include "ChartMath.h"
#include "LineChart.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "TempChart.h"
#include "ThemeManager.h"
#include "Units.h"

ChartsView::ChartsView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    m_temp = new TempChart;
    m_rain = new BarChart;
    m_snow = new BarChart;
    m_line = new LineChart;

    // Page 0: temperature on the left, rain and snow stacked on the right
    auto *right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(10);
    right->addWidget(m_rain, 1);
    right->addWidget(m_snow, 1);

    auto *tempPage = new QWidget;
    auto *tempLayout = new QHBoxLayout(tempPage);
    tempLayout->setContentsMargins(0, 0, 0, 0);
    tempLayout->setSpacing(10);
    tempLayout->addWidget(m_temp, 2);               // roughly two-thirds of the width
    tempLayout->addLayout(right, 1);                // rain/snow stacked in the last third

    // Page 1: one chart for wind, pressure or humidity
    m_pages = new QStackedWidget;
    m_pages->addWidget(tempPage);
    m_pages->addWidget(m_line);

    // The row of buttons that chooses which chart to show
    m_picker = new QButtonGroup(this);
    auto *pickRow = new QHBoxLayout;
    pickRow->setContentsMargins(0, 0, 0, 0);
    pickRow->addWidget(new QLabel("Chart:"));
    const QStringList names = {"Temperature and rain", "Wind", "Pressure", "Humidity"};
    for (int i = 0; i < names.size(); ++i) {
        auto *button = new QPushButton(names[i]);
        button->setCheckable(true);
        button->setMinimumHeight(44);
        m_picker->addButton(button, i);
        pickRow->addWidget(button);
    }
    pickRow->addStretch(1);
    m_picker->button(0)->setChecked(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(pickRow);
    layout->addWidget(m_pages, 1);

    connect(m_picker, &QButtonGroup::idClicked, this, [this] {
        if (isVisible()) reload();
    });
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible()) reload();
    });
    connect(&m_state, &AppState::periodChanged, this, [this](Tab tab) {
        if (tab == Tab::Charts && isVisible()) reload();
    });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        if (isVisible()) reload();
    });
}

void ChartsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();                                   // data or units may have changed while hidden
}

void ChartsView::reload()
{
    const Period p = m_state.period(Tab::Charts);
    const QDate from = PeriodMath::startOf(m_state.currentDate(), p);
    const QDate to = PeriodMath::endOf(m_state.currentDate(), p);

    const auto days = m_repo.weatherInRange(from, to);          // ONE query for the period
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));
    const auto theme = ThemeManager::instance()->colors();

    const int chart = m_picker->checkedId();
    m_pages->setCurrentIndex(chart == 0 ? 0 : 1);

    if (chart == 0) {
        m_temp->setData(ChartMath::temperatureSeries(from, to, days, units), p, units);

        const auto rain = ChartMath::precipSeries(from, to, p, days,
                                                  ChartMath::PrecipKind::Rain, units);
        const auto snow = ChartMath::precipSeries(from, to, p, days,
                                                  ChartMath::PrecipKind::Snow, units);
        m_rain->setData(QString(p == Period::Year ? "Rain per month (%1)" : "Rain per day (%1)")
                            .arg(Units::rainUnit(units)), rain, theme.rain);
        m_snow->setData(QString(p == Period::Year ? "Snow per month (%1)" : "Snow per day (%1)")
                            .arg(Units::snowUnit(units)), snow, theme.snow);
        m_snow->setVisible(snow.hasAnyBar);         // only when the period has snow
        return;
    }

    if (chart == 1) {
        m_line->setData(QString("Wind (%1)").arg(Units::windUnit(units)),
                        ChartMath::metricSeries(from, to, days, ChartMath::Metric::Wind, units),
                        {theme.low, theme.high}, p);
    } else if (chart == 2) {
        m_line->setData(QString("Pressure (%1)").arg(Units::pressureUnit(units)),
                        ChartMath::metricSeries(from, to, days, ChartMath::Metric::Pressure, units),
                        {theme.markNote}, p);
    } else {
        m_line->setData("Humidity (%)",
                        ChartMath::metricSeries(from, to, days, ChartMath::Metric::Humidity, units),
                        {theme.markEvent}, p);
    }
}
