#pragma once

#include <QDate>
#include <QWidget>

#include "Units.h"

class AppState;
class Card;
class QBoxLayout;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QShowEvent;
class QVBoxLayout;
class Repository;
class WeekStrip;

// One day, read only (plan 2.3):
// - a Weather card: the banner picture (soft looks, IL1), the condition as a heading with
//   [Edit this day] beside it, and the details in two columns (one when the card is narrow);
// - a Note card, an Events card and "On this day in earlier years", beside the Weather card
//   in Wide windows (RZ9) and below it otherwise; the cards stretch with the window (RZ10);
// - "This week" under the Weather card: the day's week as seven tiles that open their day
//   (D63); under the empty state on an empty day;
// - a quiet [Delete day] at the bottom right, apart from the rest (B4).
// An empty day shows EM1's empty state instead (IL2).
class DayView : public QWidget
{
    Q_OBJECT

public:
    DayView(AppState &state, Repository &repo, QWidget *parent = nullptr);

    void reload() { if (isVisible()) refresh(); }   // call after saving (Stage 7)

    bool sideBySide() const { return m_sideBySide; }    // for the layout check

signals:
    void editRequested(const QDate &date);
    void deleteRequested(const QDate &date);   // "Delete day" (the window offers Undo)

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh();
    void arrange();                        // the cards side by side or stacked; details columns
    void refreshEarlierYears(const QDate &date, Units::System units);
    void placeWeek(bool emptyDay);

    AppState &m_state;
    Repository &m_repo;
    QWidget *m_filled = nullptr;           // the cards and Delete day
    QWidget *m_empty = nullptr;            // EM1: shown instead when nothing is recorded
    QBoxLayout *m_cardsBox = nullptr;
    QWidget *m_leftColumn = nullptr, *m_rightColumn = nullptr;
    Card *m_weatherCard = nullptr, *m_noteCard = nullptr, *m_eventsCard = nullptr;
    QWidget *m_banner = nullptr;           // the day's picture (soft looks only)
    QLabel *m_condition = nullptr;         // "Snow · High 1 °C · Low −4 °C"
    QPushButton *m_editButton = nullptr;
    QWidget *m_details = nullptr;
    QHBoxLayout *m_detailColumnsBox = nullptr;
    QGridLayout *m_detailGrids[2] = {nullptr, nullptr};   // the details' two columns
    QList<QLabel *> m_detailLabels, m_detailValues;
    QLabel *m_noteText = nullptr;
    QGridLayout *m_eventsGrid = nullptr;   // two columns, room for every event
    QList<QLabel *> m_eventLabels;         // one per event type, in eventTypes() order
    QLabel *m_eventsEmpty = nullptr;
    Card *m_earlierCard = nullptr;         // "On this day in earlier years"
    QLabel *m_earlierEmpty = nullptr;
    QGridLayout *m_earlierGrid = nullptr;
    QWidget *m_week = nullptr;             // "This week" (D63): its heading and the strip
    WeekStrip *m_weekStrip = nullptr;
    QVBoxLayout *m_rootLayout = nullptr, *m_leftLayout = nullptr;
    bool m_weekPlaced = false, m_weekUnderEmpty = false;
    QPushButton *m_deleteButton = nullptr;
    QLabel *m_emptyPicture = nullptr;
    bool m_sideBySide = false;
    bool m_arranged = false;
    int m_detailColumns = 0;
};
