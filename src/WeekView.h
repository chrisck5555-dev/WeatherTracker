#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>

#include "Units.h"
#include "WeatherDay.h"

class AppState;
class HoverFade;
class Repository;

// Seven painted rows, Sunday first. One widget, no per-row children.
class WeekView : public QWidget
{
    Q_OBJECT

public:
    WeekView(AppState &state, Repository &repo, QWidget *parent = nullptr);

signals:
    void dayActivated(const QDate &date);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;                 // claims Up/Down
    void keyPressEvent(QKeyEvent *event) override;      // Up/Down move a day, Enter opens it
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    QSize minimumSizeHint() const override { return QSize(620, 420); }

private:
    void reload();
    void updateAccessibleText();
    QDate weekStart() const;
    QRectF rowRect(int row) const;

    AppState &m_state;
    Repository &m_repo;
    QDate m_loadedStart;
    Units::System m_units = Units::System::Metric;
    QMap<QDate, WeatherDay> m_weather;
    QSet<QDate> m_noteDates, m_eventDates;
    int m_pressed = -1;                         // row being pressed
    HoverFade *m_hover = nullptr;               // ST2's tint under the mouse
};