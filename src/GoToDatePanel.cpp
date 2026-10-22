#include "GoToDatePanel.h"

#include <QCalendarWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ThemeManager.h"
#include "WeatherInput.h"

namespace {
QString shown(const QDate &d) { return d.toString("MMMM d, yyyy"); }
}

GoToDatePanel::GoToDatePanel(const QDate &start, QWidget *parent)
    : QWidget(parent)
{
    QFont f = font();
    f.setPointSize(13);
    setFont(f);

    auto *title = new QLabel("Go to date");
    QFont tf = font();
    tf.setPointSize(17);
    tf.setBold(true);
    title->setFont(tf);

    auto *prompt = new QLabel("Type a date, or click one in the calendar:");

    m_input = new QLineEdit(shown(start));
    m_input->setMinimumHeight(48);
    m_input->setPlaceholderText("For example: 2026-10-21 or October 21, 2026");
    m_input->selectAll();

    m_error = new QLabel;
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QString("color: %1; font-weight: bold;")
                               .arg(ThemeManager::instance()->colors().error.name()));
    m_error->hide();

    m_calendar = new QCalendarWidget;
    m_calendar->setGridVisible(true);
    m_calendar->setFirstDayOfWeek(Qt::Sunday);
    m_calendar->setSelectedDate(start);
    m_calendar->setMinimumHeight(260);

    auto *cancel = new QPushButton("Cancel");
    auto *go = new QPushButton("Go to date");
    for (auto *b : {cancel, go}) {
        b->setMinimumSize(160, 52);
        b->setAutoDefault(false);
    }
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);
    buttons->addStretch(1);
    buttons->addWidget(go);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);
    layout->addWidget(prompt);
    layout->addWidget(m_input);
    layout->addWidget(m_error);
    layout->addWidget(m_calendar, 1);
    layout->addLayout(buttons);

    // Typing moves the calendar (when the text is a date); clicking fills the box.
    connect(m_input, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_error->hide();
        if (const auto d = WeatherInput::parseDate(text)) {
            const QSignalBlocker block(m_calendar);
            m_calendar->setSelectedDate(*d);
        }
    });
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, [this] {
        const QSignalBlocker block(m_input);
        m_input->setText(shown(m_calendar->selectedDate()));
        m_error->hide();
    });
    connect(m_input, &QLineEdit::returnPressed, this, &GoToDatePanel::tryAccept);
    connect(go, &QPushButton::clicked, this, &GoToDatePanel::tryAccept);
    connect(cancel, &QPushButton::clicked, this, &GoToDatePanel::cancelled);

    setFocusProxy(m_input);
}

void GoToDatePanel::tryAccept()
{
    const auto d = WeatherInput::parseDate(m_input->text());
    if (!d) {
        m_error->setText("⚠ That is not a date I can read. "
                         "Try 2026-10-21 or October 21, 2026.");
        m_error->show();
        return;
    }
    emit dateChosen(*d);
}
