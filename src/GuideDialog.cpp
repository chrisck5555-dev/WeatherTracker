#include "GuideDialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

GuideDialog::GuideDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("User guide");
    resize(820, 660);

    QFont f = font();
    f.setPointSize(13);
    setFont(f);

    auto *text = new QTextBrowser;
    text->setOpenExternalLinks(false);
    text->setHtml(
        "<h2>Writing down the weather</h2>"
        "<p>Press <b>Add today's weather</b> (next to the tabs). Fill in what you know and "
        "leave the rest. Empty fields show as “Not recorded”. Press <b>Save</b> when "
        "you are done, or <b>Cancel</b> to leave without saving.</p>"
        "<p>For another day, open it on the Calendar tab and press <b>Add / edit this day</b>. "
        "Besides the weather, each day has a <b>note</b> for anything you want to remember and "
        "a list of <b>events</b>, such as the first snow.</p>"

        "<h2>Looking at your days</h2>"
        "<p>On the <b>Calendar</b> tab, <b>Show</b> chooses a Day, a Week, a Month or a Year. "
        "Click a day to open it. <b>Previous</b> and <b>Next</b> move through time, and "
        "<b>Go to today</b> comes back. After opening a day, the <b>Back</b> button returns "
        "to where you were.</p>"

        "<h2>Analytics</h2>"
        "<p>Averages and totals for a day, a month, a season or a year, and what the "
        "weather was like on the same day in earlier years.</p>"

        "<h2>Charts</h2>"
        "<p>Pick <b>Temperature and rain</b>, <b>Wind</b>, <b>Pressure</b> or <b>Humidity</b>. "
        "Gaps in a line are days without a recording.</p>"

        "<h2>Records</h2>"
        "<p>The hottest, coldest, wettest, snowiest and windiest days, the highest gust, the "
        "longest dry and wet streaks, and how many days were above 25 °C or below "
        "-35 °C. Choose a month, a season, a year or all time. A row with an arrow opens "
        "that day when you click it.</p>"

        "<h2>Search and Go to date</h2>"
        "<p><b>Search</b> finds days by the words in their notes or by their events. "
        "<b>Go to date</b> jumps to any date: type it, like 2026-10-21 or October 21, 2026, "
        "or click it in the calendar.</p>"

        "<h2>Keyboard shortcuts</h2>"
        "<p><b>Left</b> and <b>Right</b> arrows: previous and next. <b>T</b>: today. "
        "<b>Ctrl+F</b>: Search. <b>Ctrl+G</b>: Go to date. <b>F1</b>: this guide.</p>"

        "<h2>Settings</h2>"
        "<p>Choose metric or imperial units, the text size, and the look of the program "
        "(soft or high contrast, light or dark). Your records are always stored in metric, "
        "so switching units never changes them.</p>"

        "<h2>Backups, and moving your data</h2>"
        "<p>The program saves a backup of your data every day, in "
        "<i>Documents\\Weather Tracker backups</i>, and keeps the last 10 days. "
        "In <b>Settings</b> you can back up right now, go back to an older backup, or "
        "<b>Save a copy</b> on a USB stick and <b>Load a copy</b> on another computer.</p>"

        "<h2>If something goes wrong</h2>"
        "<p>In Settings, press <b>Show my problem report</b> and attach the file it shows to "
        "an email. It never contains your weather values or notes.</p>");

    auto *close = new QPushButton("Close");
    close->setMinimumSize(140, 52);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(text, 1);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}
