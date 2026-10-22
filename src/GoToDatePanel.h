#pragma once

#include <QDate>
#include <QWidget>

class QCalendarWidget;
class QLabel;
class QLineEdit;

// Asks for a date: typed in a box, or clicked in a calendar. The two stay in step.
// Shown inside the main window by PanelHost.
class GoToDatePanel : public QWidget
{
    Q_OBJECT

public:
    explicit GoToDatePanel(const QDate &start, QWidget *parent = nullptr);

signals:
    void dateChosen(const QDate &date);
    void cancelled();

private:
    void tryAccept();

    QLineEdit *m_input = nullptr;
    QLabel *m_error = nullptr;
    QCalendarWidget *m_calendar = nullptr;
};
