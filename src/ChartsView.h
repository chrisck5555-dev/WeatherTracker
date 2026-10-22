#pragma once

#include <QWidget>

class AppState;
class BarChart;
class LineChart;
class QButtonGroup;
class QStackedWidget;
class Repository;
class TempChart;

// The Charts tab content. Owns the queries; the chart widgets only draw.
class ChartsView : public QWidget
{
    Q_OBJECT

public:
    ChartsView(AppState &state, Repository &repo, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void reload();

    AppState &m_state;
    Repository &m_repo;
    QButtonGroup *m_picker = nullptr;
    QStackedWidget *m_pages = nullptr;
    TempChart *m_temp = nullptr;
    BarChart *m_rain = nullptr;
    BarChart *m_snow = nullptr;
    LineChart *m_line = nullptr;
};
