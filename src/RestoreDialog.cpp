#include "RestoreDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include "Backups.h"

RestoreDialog::RestoreDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Restore from backup");
    resize(720, 520);

    QFont f = font();
    f.setPointSize(13);
    setFont(f);

    const auto backups = Backups::list();

    auto *prompt = new QLabel(backups.isEmpty()
        ? "There are no backups yet. The program makes one every day."
        : "Choose a backup to go back to. Newest first:");
    prompt->setWordWrap(true);

    m_list = new QListWidget;
    for (const Backups::Entry &e : backups) {
        const auto &c = e.counts;
        QString text = e.day.toString("dddd, MMMM d, yyyy")
            + QString("\n%1 days of weather, %2 notes. Saved %3")
                  .arg(c.weatherDays).arg(c.noteDays)
                  .arg(QLocale().toString(e.saved, "MMM d, h:mm AP"));
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, e.path);
        item->setData(Qt::UserRole + 1, "the backup from " + e.day.toString("MMMM d, yyyy")
                                        + " (" + QString::number(c.weatherDays) + " days of weather)");
        item->setSizeHint(QSize(0, 70));
        m_list->addItem(item);
    }

    auto *cancel = new QPushButton("Cancel");
    m_restore = new QPushButton("Restore this backup");
    for (auto *b : {cancel, m_restore}) {
        b->setMinimumSize(180, 52);
        b->setAutoDefault(false);
    }
    m_restore->setEnabled(false);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);                 // safe choice on the left
    buttons->addStretch(1);
    buttons->addWidget(m_restore);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(prompt);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        m_restore->setEnabled(item != nullptr);
    });
    connect(m_restore, &QPushButton::clicked, this, [this] {
        if (auto *item = m_list->currentItem()) {
            m_selected = item->data(Qt::UserRole).toString();
            m_description = item->data(Qt::UserRole + 1).toString();
            accept();
        }
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}
