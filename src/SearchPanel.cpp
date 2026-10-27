#include "SearchPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "EventTypes.h"
#include "Repository.h"

namespace {

constexpr int kMaxResults = 200;

// A short piece of the note around the first match, on one line.
QString snippet(const QString &note, const QString &needle)
{
    const QString flat = note.simplified();
    const int at = flat.indexOf(needle, 0, Qt::CaseInsensitive);
    constexpr int kLength = 110;
    int start = 0;
    if (at > 40)
        start = at - 40;
    QString piece = flat.mid(start, kLength);
    if (start > 0)
        piece = QStringLiteral("…") + piece;
    if (start + kLength < flat.size())
        piece += QStringLiteral("…");
    return piece;
}

} // namespace

SearchPanel::SearchPanel(Repository &repo, QWidget *parent)
    : QWidget(parent), m_repo(repo)
{
    QFont f = font();
    f.setPointSize(13);
    setFont(f);

    auto *title = new QLabel("Search notes and events");
    QFont tf = font();
    tf.setPointSize(17);
    tf.setBold(true);
    title->setFont(tf);

    auto *prompt = new QLabel("Type a word or phrase from a note, or the name of an event:");
    prompt->setWordWrap(true);

    m_input = new QLineEdit;
    m_input->setMinimumHeight(48);
    m_input->setClearButtonEnabled(true);
    m_input->setPlaceholderText("For example: frost, garden, first snow");

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);       // it repeats what she typed: never read it as HTML

    m_list = new QListWidget;
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(false);

    auto *close = new QPushButton("Close");
    close->setMinimumSize(140, 52);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);
    layout->addWidget(prompt);
    layout->addWidget(m_input);
    layout->addWidget(m_status);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    // Search a moment after she stops typing, so every key does not run a query.
    m_delay = new QTimer(this);
    m_delay->setSingleShot(true);
    m_delay->setInterval(300);
    connect(m_delay, &QTimer::timeout, this, &SearchPanel::runSearch);
    connect(m_input, &QLineEdit::textChanged, m_delay, qOverload<>(&QTimer::start));
    connect(m_input, &QLineEdit::returnPressed, this, &SearchPanel::runSearch);

    auto choose = [this](QListWidgetItem *item) {
        if (m_done || !item)
            return;
        m_done = true;
        emit dayChosen(item->data(Qt::UserRole).toDate());
    };
    connect(m_list, &QListWidget::itemClicked, this, choose);       // fires on release
    connect(m_list, &QListWidget::itemActivated, this, choose);     // Enter key
    connect(close, &QPushButton::clicked, this, &SearchPanel::closeRequested);

    m_status->setText("Nothing searched yet.");
    setFocusProxy(m_input);
}

void SearchPanel::runSearch()
{
    m_delay->stop();
    m_list->clear();
    const QString needle = m_input->text().trimmed();
    if (needle.isEmpty()) {
        m_status->setText("Nothing searched yet.");
        return;
    }

    struct Hit {
        QString noteSnippet;
        QStringList events;
    };
    QMap<QDate, Hit> hits;                         // sorted by date

    for (const auto &n : m_repo.searchNotes(needle, kMaxResults))
        hits[n.date].noteSnippet = snippet(n.text, needle);

    QStringList keys;
    for (const EventType &e : eventTypes())
        if (e.label.contains(needle, Qt::CaseInsensitive))
            keys << e.key;
    for (const QDate &d : m_repo.datesWithEventKeys(keys, kMaxResults)) {
        QStringList labels;
        const QSet<QString> onDay = m_repo.events(d);
        for (const EventType &e : eventTypes())    // fixed order, as everywhere else
            if (onDay.contains(e.key))
                labels << e.label;
        hits[d].events = labels;
    }

    if (hits.isEmpty()) {
        m_status->setText("No days found for “" + needle + "”.");
        return;
    }

    int shown = 0;
    for (auto it = hits.constEnd(); it != hits.constBegin() && shown < kMaxResults; ++shown) {
        --it;                                      // newest day first
        QString text = it.key().toString("dddd, MMMM d, yyyy");
        if (!it->noteSnippet.isEmpty())
            text += "\nNote: " + it->noteSnippet;
        if (!it->events.isEmpty())
            text += "\nEvents: " + it->events.join(", ");
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, it.key());
        item->setSizeHint(QSize(0, 20 + 26 * int(text.count('\n') + 1)));
        m_list->addItem(item);
    }
    m_status->setText(shown >= kMaxResults
        ? QString("Showing the newest %1 days. Type more to narrow it down.").arg(kMaxResults)
        : QString("%1 day(s) found. Click one to open it.").arg(shown));
}
