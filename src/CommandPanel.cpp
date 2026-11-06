#include "CommandPanel.h"
#include "Typography.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

CommandPanel::CommandPanel(QList<Command> commands, QWidget *parent)
    : QWidget(parent)
    , m_commands(std::move(commands))
{
    QFont f = font();
    f = textFont(Design::TextStyle::Body, f);
    setFont(f);

    auto *title = new QLabel("What would you like to do?");
    QFont tf = font();
    tf = textFont(Design::TextStyle::Body, tf);
    tf = semibold(tf);
    title->setFont(tf);

    m_input = new QLineEdit;
    m_input->setMinimumHeight(48);
    m_input->setClearButtonEnabled(true);
    m_input->setPlaceholderText("Type a few letters, for example: chart, backup, week");
    m_input->setAccessibleName("Find a command");
    m_input->installEventFilter(this);              // Up, Down and Enter move in the list

    m_list = new QTreeWidget;
    m_list->setColumnCount(2);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->setAccessibleName("Commands");
    for (int i = 0; i < m_commands.size(); ++i) {
        auto *item = new QTreeWidgetItem(m_list, {m_commands[i].label, m_commands[i].keys});
        item->setData(0, Qt::UserRole, i);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setSizeHint(0, QSize(0, 44));
    }

    auto *hint = new QLabel("The keys on the right also work without this menu.");
    hint->setWordWrap(true);
    auto *close = new QPushButton("Close");
    close->setMinimumSize(140, 52);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(hint, 1);
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);
    layout->addWidget(m_input);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPanel::filter);
    connect(m_list, &QTreeWidget::itemActivated, this, &CommandPanel::runCurrent);   // Enter, double-click
    connect(m_list, &QTreeWidget::itemClicked, this, &CommandPanel::runCurrent);     // one click
    connect(close, &QPushButton::clicked, this, &CommandPanel::closeRequested);
    filter(QString());
    setFocusProxy(m_input);
}

void CommandPanel::filter(const QString &text)
{
    // Every typed word must appear somewhere in the label, the key or the extra words.
    const QStringList typed = text.simplified().split(' ', Qt::SkipEmptyParts);
    QTreeWidgetItem *first = nullptr;
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        const Command &c = m_commands[item->data(0, Qt::UserRole).toInt()];
        const QString haystack = c.label + ' ' + c.keys + ' ' + c.words;
        bool match = true;
        for (const QString &word : typed)
            match = match && haystack.contains(word, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match && !first)
            first = item;
    }
    m_list->setCurrentItem(first);                  // Enter runs the best match
}

void CommandPanel::runCurrent()
{
    QTreeWidgetItem *item = m_list->currentItem();
    if (!item || item->isHidden())
        return;
    emit chosen(m_commands[item->data(0, Qt::UserRole).toInt()]);
}

bool CommandPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            runCurrent();
            return true;
        }
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            // Move the highlight while the typing stays in the box.
            QTreeWidgetItem *item = m_list->currentItem();
            do {
                item = item ? (key == Qt::Key_Down ? m_list->itemBelow(item) : m_list->itemAbove(item))
                            : m_list->topLevelItem(0);
            } while (item && item->isHidden());
            if (item)
                m_list->setCurrentItem(item);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
