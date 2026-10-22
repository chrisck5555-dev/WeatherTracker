#pragma once

#include <QList>
#include <QWidget>

#include "History.h"
#include "StatsRows.h"

class AppState;
class QPainter;
class Repository;

// The painted statistics panel for the Analytics tab.
class StatsView : public QWidget
{
    Q_OBJECT

public:
    StatsView(AppState &state, Repository &repo, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void reload();
    void updateHeight();
    int layoutContent(QPainter *painter) const;   // draws if painter set; returns height

    AppState &m_state;
    Repository &m_repo;
    QList<StatsRows::Row> m_rows;
    History::DayComparison m_history;
    bool m_hasHistory = false;
    History::PeriodComparison m_periodHistory;
    bool m_hasPeriodHistory = false;
};