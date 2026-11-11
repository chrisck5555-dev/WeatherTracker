#pragma once

#include <QWidget>

#include "Period.h"

class FlowLayout;
class QDate;
class QPushButton;
class TitleButton;
class TitleLabel;
struct ColumnText;

// The top of each page (plan 2.1): [Previous] [Next] [Today], then the period as a title,
// and "Back to ..." at the right end when she opened this from somewhere else.
// - The title zooms out one level on the Calendar's Day, Week and Month (CAL2): it's then a
//   quiet button with an up arrow after the text. Elsewhere it's plain text.
// - In a narrow window the title and Back move to the next line (RZ2).
class PageHeader : public QWidget
{
    Q_OBJECT

public:
    explicit PageHeader(QWidget *parent = nullptr);

    QPushButton *previousButton() const { return m_prev; }
    QPushButton *nextButton() const { return m_next; }
    QPushButton *todayButton() const { return m_today; }
    QPushButton *backButton() const { return m_back; }
    QPushButton *titleButton() const;

    // `zoomTip` empty: plain text. Otherwise the title is a button with that tooltip.
    void setTitle(const QString &text, const QString &zoomTip = QString());
    // The period's title in columns (T5), for every period and tab: the words that change as
    // she steps ("Tuesday, November"; "Week of November"; "November"; "Winter") end at a fixed
    // point, the end of a slot as wide as their widest form, and the numbers after them always
    // start at that same pixel. A one-digit day is padded to two digits' width. So the day,
    // the year and the up arrow never move.
    void setPeriodTitle(const QDate &date, Period period, const QString &zoomTip = QString());
    void setNavigationShown(bool shown);    // hidden on "All time"
    void setBack(const QString &text);      // empty: no Back button

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

signals:
    void zoomOutRequested();

private:
    void showTitle(const ColumnText &text, const QString &plain, const QString &zoomTip);

    FlowLayout *m_flow;
    QWidget *m_nav;
    QPushButton *m_prev, *m_next, *m_today, *m_back;
    TitleButton *m_titleButton;             // the title when it zooms out (CAL2)
    TitleLabel *m_titleLabel;               // the title as plain text
};
